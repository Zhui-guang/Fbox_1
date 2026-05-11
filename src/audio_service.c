#include "audio_service.h"
#include "debug_log.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_i2s.h"

#define AUDIO_DEFAULT_ENABLED         0U
#define AUDIO_DEFAULT_VOLUME_PERCENT  1U
#define AUDIO_SAMPLE_RATE_HZ          22050U
#define AUDIO_DMA_FRAMES_PER_HALF     128U
#define AUDIO_EVENT_QUEUE_SIZE        16U
#define AUDIO_PCM_FIFO_SAMPLES        4096U

#define AUDIO_I2S_WS_GPIO_PORT        GPIOA
/* PA4 is reserved for onboard W25Q128 SPI Flash CS, move I2S WS to PA15. */
#define AUDIO_I2S_WS_PIN              GPIO_PIN_15
#define AUDIO_I2S_CK_GPIO_PORT        GPIOC
#define AUDIO_I2S_CK_PIN              GPIO_PIN_10
#define AUDIO_I2S_SD_GPIO_PORT        GPIOC
#define AUDIO_I2S_SD_PIN              GPIO_PIN_12

typedef struct
{
    uint16_t freq_hz;
    uint16_t dur_ms;
    uint16_t gap_ms;
} MusicNote;

typedef struct
{
    uint8_t active;
    uint8_t priority;
    uint32_t phase;
    uint32_t phase_step;
    uint32_t total_samples;
    uint32_t emitted_samples;
    int16_t amp_target;
} AudioVoice;

/* Simplified "Dong Fang Hong" phrase for demo playback. */
static const MusicNote k_demo_music[] = {
    {523U, 220U, 30U}, {659U, 220U, 30U}, {784U, 260U, 40U},
    {659U, 220U, 30U}, {523U, 220U, 30U}, {392U, 260U, 40U},
    {523U, 220U, 30U}, {659U, 220U, 30U}, {784U, 280U, 40U},
    {988U, 280U, 40U}, {784U, 260U, 40U}, {659U, 240U, 60U}
};

static uint8_t audio_enabled;
static uint8_t audio_volume_percent;
static uint8_t i2s_ready;
static I2S_HandleTypeDef hi2s3;
static DMA_HandleTypeDef hdma_spi3_tx;
static uint8_t dma_started;
static uint16_t audio_dma_buf[AUDIO_DMA_FRAMES_PER_HALF * 2U * 2U]; /* 2 halves, stereo */

static AudioVoice sfx_voice;
static AudioVoice bgm_voice;
static uint8_t music_playing;
static uint8_t music_index;
static uint32_t music_gap_until_tick;
static uint8_t pcm_stream_active;
static int16_t pcm_fifo[AUDIO_PCM_FIFO_SAMPLES];
static uint16_t pcm_fifo_head;
static uint16_t pcm_fifo_tail;
static uint16_t pcm_fifo_count;
static uint32_t pcm_underrun_count;

static SoundEvent event_q[AUDIO_EVENT_QUEUE_SIZE];
static uint8_t event_q_head;
static uint8_t event_q_tail;
static uint8_t event_q_count;
static uint32_t event_q_overflow;
static volatile uint8_t audio_dma_half_ready;
static volatile uint8_t audio_dma_full_ready;

static void audio_i2s_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;

    GPIO_InitStruct.Pin = AUDIO_I2S_WS_PIN;
    HAL_GPIO_Init(AUDIO_I2S_WS_GPIO_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = AUDIO_I2S_CK_PIN;
    HAL_GPIO_Init(AUDIO_I2S_CK_GPIO_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = AUDIO_I2S_SD_PIN;
    HAL_GPIO_Init(AUDIO_I2S_SD_GPIO_PORT, &GPIO_InitStruct);
}

static void audio_i2s_clock_init(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    PeriphClkInit.PLLI2S.PLLI2SN = 192;
    PeriphClkInit.PLLI2S.PLLI2SR = 2;
    (void)HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

static void audio_i2s_dma_init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_spi3_tx.Instance = DMA1_Stream5;
    hdma_spi3_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi3_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_spi3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_spi3_tx.Init.Mode = DMA_CIRCULAR;
    hdma_spi3_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi3_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_spi3_tx) != HAL_OK)
    {
        Debug_Log("[AUDIO] DMA init failed\r\n");
    }

    __HAL_LINKDMA(&hi2s3, hdmatx, hdma_spi3_tx);

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

static void audio_i2s_init(void)
{
    audio_i2s_clock_init();
    audio_i2s_gpio_init();
    __HAL_RCC_SPI3_CLK_ENABLE();

    hi2s3.Instance = SPI3;
    hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
    hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
    hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
    hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
    hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_22K;
    hi2s3.Init.CPOL = I2S_CPOL_LOW;
    hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
    hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
    audio_i2s_dma_init();

    if (HAL_I2S_Init(&hi2s3) == HAL_OK)
    {
        i2s_ready = 1U;
    }
    else
    {
        i2s_ready = 0U;
        Debug_Log("[AUDIO] I2S3 init failed\r\n");
    }
}

static uint8_t audio_event_priority(SoundEvent event_id)
{
    switch (event_id)
    {
    case SOUND_GAME_OVER: return 5U;
    case SOUND_HIT:       return 4U;
    case SOUND_SCORE:     return 3U;
    case SOUND_CONFIRM:   return 2U;
    case SOUND_PAUSE:     return 2U;
    case SOUND_MENU_MOVE: return 1U;
    default:              return 0U;
    }
}

static void audio_event_to_tone(SoundEvent event_id, uint16_t *freq_hz, uint16_t *dur_ms)
{
    switch (event_id)
    {
    case SOUND_MENU_MOVE: *freq_hz = 660U;  *dur_ms = 20U;  break;
    case SOUND_CONFIRM:   *freq_hz = 880U;  *dur_ms = 35U;  break;
    case SOUND_SCORE:     *freq_hz = 1040U; *dur_ms = 45U;  break;
    case SOUND_HIT:       *freq_hz = 220U;  *dur_ms = 70U;  break;
    case SOUND_PAUSE:     *freq_hz = 520U;  *dur_ms = 30U;  break;
    case SOUND_GAME_OVER: *freq_hz = 180U;  *dur_ms = 140U; break;
    default:              *freq_hz = 0U;    *dur_ms = 0U;   break;
    }
}

/* Perceptual-ish mapping: low volume region has finer control. */
static int16_t audio_volume_to_amplitude(uint8_t volume_percent)
{
    uint32_t v = (uint32_t)volume_percent;
    uint32_t amp;

    if (v == 0U)
    {
        return 0;
    }

    /* Keep low volume audible: floor + near-linear mapping. */
    amp = 180U + (v * 3200U) / 100U;  /* 1% -> ~212, 100% -> ~3380 */
    if (amp > 3800U)
    {
        amp = 3800U;
    }
    return (int16_t)amp;
}

static void audio_voice_start(AudioVoice *voice, uint16_t freq_hz, uint16_t dur_ms, uint8_t priority, int16_t amp_target)
{
    uint64_t step;

    if ((voice == (AudioVoice *)0) || (freq_hz == 0U) || (dur_ms == 0U))
    {
        return;
    }

    step = ((uint64_t)freq_hz << 32) / (uint64_t)AUDIO_SAMPLE_RATE_HZ;
    voice->phase_step = (uint32_t)step;
    voice->phase = 0U;
    voice->total_samples = ((uint32_t)AUDIO_SAMPLE_RATE_HZ * (uint32_t)dur_ms) / 1000U;
    if (voice->total_samples == 0U)
    {
        voice->total_samples = 1U;
    }
    voice->emitted_samples = 0U;
    voice->amp_target = amp_target;
    voice->priority = priority;
    voice->active = 1U;
}

/* Brighter timbre: triangle + a small square-wave component. */
static int16_t audio_voice_sample(AudioVoice *voice)
{
    uint32_t p10;
    int32_t tri;
    int32_t sqr;
    int32_t wave;
    int32_t amp;
    uint32_t attack_samples;
    uint32_t release_samples;
    uint32_t remain;
    int16_t out;

    if ((voice == (AudioVoice *)0) || (voice->active == 0U))
    {
        return 0;
    }

    p10 = voice->phase >> 22; /* 0..1023 */
    if (p10 < 512U)
    {
        tri = (int32_t)p10;
    }
    else
    {
        tri = (int32_t)(1023U - p10);
    }
    tri = (tri * 128) - 32768; /* roughly -32768..32767 */
    sqr = (p10 < 512U) ? 32767 : -32768;
    wave = (tri * 70 + sqr * 30) / 100; /* brighten while keeping smoother body */

    attack_samples = (AUDIO_SAMPLE_RATE_HZ * 2U) / 1000U;  /* 2ms: crisper attack */
    release_samples = (AUDIO_SAMPLE_RATE_HZ * 6U) / 1000U; /* 6ms: still suppress clicks */
    remain = voice->total_samples - voice->emitted_samples;

    amp = voice->amp_target;
    if ((attack_samples > 0U) && (voice->emitted_samples < attack_samples))
    {
        amp = (amp * (int32_t)voice->emitted_samples) / (int32_t)attack_samples;
    }
    else if ((release_samples > 0U) && (remain < release_samples))
    {
        amp = (amp * (int32_t)remain) / (int32_t)release_samples;
    }

    out = (int16_t)((wave * amp) / 32768);

    voice->phase += voice->phase_step;
    voice->emitted_samples++;
    if (voice->emitted_samples >= voice->total_samples)
    {
        voice->active = 0U;
        voice->priority = 0U;
    }

    return out;
}

static void audio_queue_push(SoundEvent event_id)
{
    if (event_id == SOUND_NONE)
    {
        return;
    }

    if (event_q_count >= AUDIO_EVENT_QUEUE_SIZE)
    {
        /* drop oldest on overflow */
        event_q_head = (uint8_t)((event_q_head + 1U) % AUDIO_EVENT_QUEUE_SIZE);
        event_q_count--;
        event_q_overflow++;
    }

    event_q[event_q_tail] = event_id;
    event_q_tail = (uint8_t)((event_q_tail + 1U) % AUDIO_EVENT_QUEUE_SIZE);
    event_q_count++;
}

static uint8_t audio_queue_pop_highest(SoundEvent *out_event)
{
    uint8_t i;
    uint8_t idx;
    uint8_t best_i = 0U;
    uint8_t best_pri = 0U;
    SoundEvent ev;

    if ((out_event == (SoundEvent *)0) || (event_q_count == 0U))
    {
        return 0U;
    }

    for (i = 0U; i < event_q_count; ++i)
    {
        idx = (uint8_t)((event_q_head + i) % AUDIO_EVENT_QUEUE_SIZE);
        ev = event_q[idx];
        if (audio_event_priority(ev) >= best_pri)
        {
            best_pri = audio_event_priority(ev);
            best_i = i;
        }
    }

    idx = (uint8_t)((event_q_head + best_i) % AUDIO_EVENT_QUEUE_SIZE);
    *out_event = event_q[idx];

    for (i = best_i; i + 1U < event_q_count; ++i)
    {
        uint8_t cur = (uint8_t)((event_q_head + i) % AUDIO_EVENT_QUEUE_SIZE);
        uint8_t nxt = (uint8_t)((event_q_head + i + 1U) % AUDIO_EVENT_QUEUE_SIZE);
        event_q[cur] = event_q[nxt];
    }
    event_q_tail = (uint8_t)((event_q_head + event_q_count - 1U) % AUDIO_EVENT_QUEUE_SIZE);
    event_q_count--;

    return 1U;
}

static void audio_schedule_sfx_from_queue(void)
{
    SoundEvent ev;
    uint16_t freq_hz;
    uint16_t dur_ms;
    uint8_t pri;
    int16_t amp;

    if (event_q_count == 0U)
    {
        return;
    }

    if (audio_queue_pop_highest(&ev) == 0U)
    {
        return;
    }

    pri = audio_event_priority(ev);
    if ((sfx_voice.active != 0U) && (pri < sfx_voice.priority))
    {
        /* keep current higher-priority SFX, push back this event */
        audio_queue_push(ev);
        return;
    }

    audio_event_to_tone(ev, &freq_hz, &dur_ms);
    amp = audio_volume_to_amplitude(audio_volume_percent);
    audio_voice_start(&sfx_voice, freq_hz, dur_ms, pri, amp);
}

static void audio_schedule_bgm(uint32_t now)
{
    int16_t amp;

    if (music_playing == 0U)
    {
        return;
    }

    if (bgm_voice.active != 0U)
    {
        return;
    }

    if (now < music_gap_until_tick)
    {
        return;
    }

    if (music_index >= (sizeof(k_demo_music) / sizeof(k_demo_music[0])))
    {
        music_playing = 0U;
        return;
    }

    amp = audio_volume_to_amplitude(audio_volume_percent);
    amp = (int16_t)((int32_t)amp * 55 / 100); /* BGM lower than SFX */
    audio_voice_start(&bgm_voice, k_demo_music[music_index].freq_hz, k_demo_music[music_index].dur_ms, 1U, amp);
    music_gap_until_tick = now + k_demo_music[music_index].dur_ms + k_demo_music[music_index].gap_ms;
    music_index++;
}

static void audio_render_frames(uint16_t *dst, uint32_t frames)
{
    uint32_t i;
    for (i = 0U; i < frames; ++i)
    {
        int32_t sfx = audio_voice_sample(&sfx_voice);
        int32_t bgm = 0;
        int32_t mix;
        int16_t out;

        if (pcm_stream_active != 0U)
        {
            if (pcm_fifo_count > 0U)
            {
                bgm = pcm_fifo[pcm_fifo_head];
                pcm_fifo_head = (uint16_t)((pcm_fifo_head + 1U) % AUDIO_PCM_FIFO_SAMPLES);
                pcm_fifo_count--;
                /* PCM path follows system volume in real time. */
                bgm = (bgm * (int32_t)audio_volume_percent) / 100;
                /* Keep mix headroom for concurrent SFX. */
                bgm = (bgm * 60) / 100;
            }
            else
            {
                pcm_underrun_count++;
            }
        }
        else
        {
            bgm = audio_voice_sample(&bgm_voice);
            /* ducking: when SFX active, BGM is attenuated */
            if (sfx_voice.active != 0U)
            {
                bgm = (bgm * 35) / 100;
            }
        }

        mix = sfx + bgm;
        if (mix > 32767)
        {
            mix = 32767;
        }
        else if (mix < -32768)
        {
            mix = -32768;
        }
        out = (int16_t)mix;

        dst[i * 2U] = (uint16_t)out;
        dst[i * 2U + 1U] = (uint16_t)out;
    }
}

static void audio_fill_silence(uint16_t *dst, uint32_t frames)
{
    uint32_t i;
    for (i = 0U; i < frames * 2U; ++i)
    {
        dst[i] = 0U;
    }
}

static void audio_dma_start_if_needed(void)
{
    if ((i2s_ready == 0U) || (dma_started != 0U))
    {
        return;
    }

    audio_fill_silence(audio_dma_buf, AUDIO_DMA_FRAMES_PER_HALF * 2U);
    if (HAL_I2S_Transmit_DMA(&hi2s3, audio_dma_buf, (uint16_t)(AUDIO_DMA_FRAMES_PER_HALF * 4U)) == HAL_OK)
    {
        dma_started = 1U;
        audio_dma_half_ready = 1U;
        audio_dma_full_ready = 1U;
    }
    else
    {
        Debug_Log("[AUDIO] DMA start failed\r\n");
    }
}

void Audio_Service_Init(void)
{
    audio_enabled = AUDIO_DEFAULT_ENABLED;
    audio_volume_percent = AUDIO_DEFAULT_VOLUME_PERCENT;
    i2s_ready = 0U;

    sfx_voice.active = 0U;
    sfx_voice.priority = 0U;
    bgm_voice.active = 0U;
    bgm_voice.priority = 0U;
    music_playing = 0U;
    music_index = 0U;
    music_gap_until_tick = 0U;
    pcm_stream_active = 0U;
    pcm_fifo_head = 0U;
    pcm_fifo_tail = 0U;
    pcm_fifo_count = 0U;
    pcm_underrun_count = 0U;
    event_q_head = 0U;
    event_q_tail = 0U;
    event_q_count = 0U;
    event_q_overflow = 0U;
    dma_started = 0U;
    audio_dma_half_ready = 0U;
    audio_dma_full_ready = 0U;

    audio_i2s_init();
    Debug_Log("[AUDIO] init: enabled=%u, volume=%u%%\r\n", audio_enabled, audio_volume_percent);
}

void Audio_Service_Tick(void)
{
    uint32_t now = HAL_GetTick();

    if (audio_enabled == 0U)
    {
        if (dma_started != 0U)
        {
            (void)HAL_I2S_DMAStop(&hi2s3);
            dma_started = 0U;
        }
        return;
    }

    audio_dma_start_if_needed();
    audio_schedule_sfx_from_queue();
    audio_schedule_bgm(now);

    if (audio_dma_half_ready != 0U)
    {
        audio_dma_half_ready = 0U;
        audio_render_frames(&audio_dma_buf[0], AUDIO_DMA_FRAMES_PER_HALF);
    }
    if (audio_dma_full_ready != 0U)
    {
        audio_dma_full_ready = 0U;
        audio_render_frames(&audio_dma_buf[AUDIO_DMA_FRAMES_PER_HALF * 2U], AUDIO_DMA_FRAMES_PER_HALF);
    }
}

void Audio_PostEvent(SoundEvent event_id)
{
    if ((audio_enabled == 0U) || (event_id == SOUND_NONE))
    {
        return;
    }
    audio_queue_push(event_id);
}

void Audio_Service_PlayEvent(SoundEvent event_id)
{
    Audio_PostEvent(event_id);
}

void Audio_Service_SetEnabled(uint8_t enabled)
{
    audio_enabled = (enabled != 0U) ? 1U : 0U;
    if (audio_enabled == 0U)
    {
        sfx_voice.active = 0U;
        bgm_voice.active = 0U;
        music_playing = 0U;
        pcm_stream_active = 0U;
        pcm_fifo_head = pcm_fifo_tail = pcm_fifo_count = 0U;
        event_q_head = event_q_tail = event_q_count = 0U;
        if (dma_started != 0U)
        {
            (void)HAL_I2S_DMAStop(&hi2s3);
            dma_started = 0U;
        }
    }
    Debug_Log("[AUDIO] enabled=%u\r\n", audio_enabled);
}

uint8_t Audio_Service_IsEnabled(void)
{
    return audio_enabled;
}

void Audio_Service_SetVolumePercent(uint8_t volume_percent)
{
    if (volume_percent > 100U)
    {
        volume_percent = 100U;
    }
    audio_volume_percent = volume_percent;
    Debug_Log("[AUDIO] volume=%u%%\r\n", audio_volume_percent);
}

uint8_t Audio_Service_GetVolumePercent(void)
{
    return audio_volume_percent;
}

uint8_t Audio_Service_IsReady(void)
{
    return i2s_ready;
}

void Audio_Service_PlayDemoMusic(void)
{
    if ((audio_enabled == 0U) || (i2s_ready == 0U))
    {
        return;
    }

    pcm_stream_active = 0U;
    pcm_fifo_head = pcm_fifo_tail = pcm_fifo_count = 0U;
    music_playing = 1U;
    music_index = 0U;
    music_gap_until_tick = HAL_GetTick();
}

uint8_t Audio_Service_IsMusicPlaying(void)
{
    return music_playing;
}

void Audio_Service_StopMusic(void)
{
    music_playing = 0U;
    bgm_voice.active = 0U;
    pcm_stream_active = 0U;
    pcm_fifo_head = pcm_fifo_tail = pcm_fifo_count = 0U;
}

void Audio_Service_StartPcmStream(void)
{
    if ((audio_enabled == 0U) || (i2s_ready == 0U))
    {
        return;
    }
    music_playing = 0U;
    bgm_voice.active = 0U;
    pcm_fifo_head = 0U;
    pcm_fifo_tail = 0U;
    pcm_fifo_count = 0U;
    pcm_underrun_count = 0U;
    pcm_stream_active = 1U;
    Debug_Log("[AUDIO] PCM stream start\r\n");
}

void Audio_Service_StopPcmStream(void)
{
    pcm_stream_active = 0U;
    pcm_fifo_head = pcm_fifo_tail = pcm_fifo_count = 0U;
    Debug_Log("[AUDIO] PCM stream stop (underrun=%lu)\r\n", (unsigned long)pcm_underrun_count);
}

uint16_t Audio_Service_GetPcmFreeSamples(void)
{
    return (uint16_t)(AUDIO_PCM_FIFO_SAMPLES - pcm_fifo_count);
}

uint8_t Audio_Service_IsPcmStreamActive(void)
{
    return pcm_stream_active;
}

uint16_t Audio_Service_PushPcmMono16(const int16_t *samples, uint16_t count)
{
    uint16_t pushed = 0U;
    if ((samples == (const int16_t *)0) || (count == 0U) || (pcm_stream_active == 0U))
    {
        return 0U;
    }

    while ((pushed < count) && (pcm_fifo_count < AUDIO_PCM_FIFO_SAMPLES))
    {
        pcm_fifo[pcm_fifo_tail] = samples[pushed];
        pcm_fifo_tail = (uint16_t)((pcm_fifo_tail + 1U) % AUDIO_PCM_FIFO_SAMPLES);
        pcm_fifo_count++;
        pushed++;
    }
    return pushed;
}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        audio_dma_half_ready = 1U;
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        audio_dma_full_ready = 1U;
    }
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s3)
    {
        Debug_Log("[AUDIO] I2S DMA error: 0x%08lX\r\n", (unsigned long)HAL_I2S_GetError(hi2s));
        dma_started = 0U;
    }
}

void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi3_tx);
}
