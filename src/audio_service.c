#include "audio_service.h"
#include "debug_log.h"
#include "stm32f4xx_hal_i2s.h"

/*
 * 课程阶段音频策略（安静优先）：
 * 1) 先打通“事件 -> 音频服务”的软件链路；
 * 2) 默认关闭实际输出，避免突发大音量；
 * 3) 默认音量设为较低值，后续接 I2S+MAX98357 时可直接复用。
 */
#define AUDIO_DEFAULT_ENABLED         0U
#define AUDIO_DEFAULT_VOLUME_PERCENT  1U
#define AUDIO_SAMPLE_RATE_HZ          16000U
#define AUDIO_TX_FRAMES_PER_TICK      64U

/* MAX98357 minimal I2S wiring (current plan):
 * WS/LRC  -> PA4  (I2S3_WS)
 * BCLK    -> PC10 (I2S3_CK)
 * DIN     -> PC12 (I2S3_SD)
 */
#define AUDIO_I2S_WS_GPIO_PORT        GPIOA
#define AUDIO_I2S_WS_PIN              GPIO_PIN_4
#define AUDIO_I2S_CK_GPIO_PORT        GPIOC
#define AUDIO_I2S_CK_PIN              GPIO_PIN_10
#define AUDIO_I2S_SD_GPIO_PORT        GPIOC
#define AUDIO_I2S_SD_PIN              GPIO_PIN_12

static uint8_t audio_enabled;
static uint8_t audio_volume_percent;
static I2S_HandleTypeDef hi2s3;
static uint8_t i2s_ready;

static uint8_t tone_active;
static uint32_t tone_freq_hz;
static uint32_t tone_remain_samples;
static uint32_t tone_half_period_samples;
static uint32_t tone_phase_samples;
static int16_t tone_level;
static int16_t tone_amp;
static uint8_t music_playing;
static uint8_t music_index;
static uint32_t music_gap_until_tick;

typedef struct
{
    uint16_t freq_hz;
    uint16_t dur_ms;
    uint16_t gap_ms;
} MusicNote;

/* 东方红（简化片段，课程演示版） */
static const MusicNote k_demo_music[] = {
    {523U, 220U, 30U}, {659U, 220U, 30U}, {784U, 260U, 40U},
    {659U, 220U, 30U}, {523U, 220U, 30U}, {392U, 260U, 40U},
    {523U, 220U, 30U}, {659U, 220U, 30U}, {784U, 280U, 40U},
    {988U, 280U, 40U}, {784U, 260U, 40U}, {659U, 240U, 60U}
};

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

    /* 16k 提示音不要求高保真，PLLI2S 参数使用稳定通用配置。 */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    PeriphClkInit.PLLI2S.PLLI2SN = 192;
    PeriphClkInit.PLLI2S.PLLI2SR = 2;
    (void)HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
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
    hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_16K;
    hi2s3.Init.CPOL = I2S_CPOL_LOW;
    hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
    hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;

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

static void audio_start_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    uint32_t half_period;
    uint32_t samples_total;
    uint32_t scaled;

    if (freq_hz == 0U)
    {
        return;
    }

    samples_total = (AUDIO_SAMPLE_RATE_HZ * duration_ms) / 1000U;
    if (samples_total == 0U)
    {
        samples_total = 1U;
    }

    half_period = AUDIO_SAMPLE_RATE_HZ / (freq_hz * 2U);
    if (half_period == 0U)
    {
        half_period = 1U;
    }

    /* 非线性缩放无需复杂算法，保持低音量基线。 */
    scaled = (uint32_t)audio_volume_percent * 120U;
    if (scaled > 3000U)
    {
        scaled = 3000U;
    }

    tone_freq_hz = freq_hz;
    tone_remain_samples = samples_total;
    tone_half_period_samples = half_period;
    tone_phase_samples = 0U;
    tone_amp = (int16_t)scaled;
    tone_level = tone_amp;
    tone_active = 1U;
}

static void audio_emit_chunk(void)
{
    uint16_t tx_buf[AUDIO_TX_FRAMES_PER_TICK * 2U];
    uint32_t frames = AUDIO_TX_FRAMES_PER_TICK;
    uint32_t i;
    int16_t s = 0;

    if ((i2s_ready == 0U) || (tone_active == 0U))
    {
        return;
    }

    if (tone_remain_samples < frames)
    {
        frames = tone_remain_samples;
    }
    if (frames == 0U)
    {
        tone_active = 0U;
        return;
    }

    for (i = 0U; i < frames; ++i)
    {
        s = tone_level;
        tx_buf[i * 2U] = (uint16_t)s;         /* Left */
        tx_buf[i * 2U + 1U] = (uint16_t)s;    /* Right */

        tone_phase_samples++;
        if (tone_phase_samples >= tone_half_period_samples)
        {
            tone_phase_samples = 0U;
            tone_level = (int16_t)(-tone_level);
        }
    }

    (void)HAL_I2S_Transmit(&hi2s3, tx_buf, (uint16_t)(frames * 2U), 30U);

    tone_remain_samples -= frames;
    if (tone_remain_samples == 0U)
    {
        tone_active = 0U;
    }
}

static const char *audio_event_to_string(SoundEvent event_id)
{
    switch (event_id)
    {
    case SOUND_MENU_MOVE:
        return "MENU_MOVE";
    case SOUND_CONFIRM:
        return "CONFIRM";
    case SOUND_SCORE:
        return "SCORE";
    case SOUND_HIT:
        return "HIT";
    case SOUND_PAUSE:
        return "PAUSE";
    case SOUND_GAME_OVER:
        return "GAME_OVER";
    default:
        return "NONE";
    }
}

void Audio_Service_Init(void)
{
    audio_enabled = AUDIO_DEFAULT_ENABLED;
    audio_volume_percent = AUDIO_DEFAULT_VOLUME_PERCENT;
    tone_active = 0U;
    i2s_ready = 0U;
    music_playing = 0U;
    music_index = 0U;
    music_gap_until_tick = 0U;
    audio_i2s_init();
    Debug_Log("[AUDIO] init: enabled=%u, volume=%u%%\r\n", audio_enabled, audio_volume_percent);
}

void Audio_Service_Tick(void)
{
    if (audio_enabled != 0U)
    {
        audio_emit_chunk();

        if ((music_playing != 0U) && (tone_active == 0U))
        {
            uint32_t now = HAL_GetTick();
            if (now >= music_gap_until_tick)
            {
                if (music_index < (sizeof(k_demo_music) / sizeof(k_demo_music[0])))
                {
                    const MusicNote *n = &k_demo_music[music_index++];
                    audio_start_tone(n->freq_hz, n->dur_ms);
                    music_gap_until_tick = now + n->dur_ms + n->gap_ms;
                }
                else
                {
                    music_playing = 0U;
                }
            }
        }
    }
}

void Audio_Service_PlayEvent(SoundEvent event_id)
{
    if ((audio_enabled == 0U) || (event_id == SOUND_NONE))
    {
        return;
    }

    switch (event_id)
    {
    case SOUND_MENU_MOVE:
        audio_start_tone(660U, 20U);
        break;
    case SOUND_CONFIRM:
        audio_start_tone(880U, 35U);
        break;
    case SOUND_SCORE:
        audio_start_tone(1040U, 45U);
        break;
    case SOUND_HIT:
        audio_start_tone(220U, 70U);
        break;
    case SOUND_PAUSE:
        audio_start_tone(520U, 30U);
        break;
    case SOUND_GAME_OVER:
        audio_start_tone(180U, 140U);
        break;
    default:
        break;
    }

    Debug_Log("[AUDIO] event=%s, volume=%u%%\r\n", audio_event_to_string(event_id), audio_volume_percent);
}

void Audio_Service_SetEnabled(uint8_t enabled)
{
    audio_enabled = (enabled != 0U) ? 1U : 0U;
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

void Audio_Service_PlayDemoMusic(void)
{
    if ((audio_enabled == 0U) || (i2s_ready == 0U))
    {
        return;
    }

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
    tone_active = 0U;
}
