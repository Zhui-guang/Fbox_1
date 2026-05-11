#include "player_service.h"
#include "audio_service.h"
#include "debug_log.h"
#include "w25q128.h"

typedef struct
{
    uint8_t current_track_id;
    uint32_t position_ms;
    uint8_t volume_percent;
    PlayerState state;
    PlayerRepeatMode repeat_mode;
    uint32_t last_tick;
    uint8_t pcm_active;
    uint32_t pcm_flash_addr;
    uint32_t pcm_total_bytes;
    uint32_t pcm_offset_bytes;
} PlayerContext;

static PlayerContext g_player;
static uint8_t g_pcm_read_buf[512];
static int16_t g_pcm_sample_buf[256];

static uint8_t player_find_track_index_by_id(uint8_t id)
{
    uint8_t i;
    uint8_t n = Music_Library_GetCount();
    for (i = 0U; i < n; ++i)
    {
        const TrackMeta *t = Music_Library_GetByIndex(i);
        if ((t != (const TrackMeta *)0) && (t->id == id))
        {
            return i;
        }
    }
    return 0U;
}

void Player_Init(void)
{
    const TrackMeta *t0 = Music_Library_GetByIndex(0U);
    g_player.current_track_id = (t0 != (const TrackMeta *)0) ? t0->id : 0U;
    g_player.position_ms = 0U;
    g_player.volume_percent = 100U;
    g_player.state = PLAYER_STOPPED;
    g_player.repeat_mode = PLAYER_REPEAT_OFF;
    g_player.last_tick = HAL_GetTick();
    g_player.pcm_active = 0U;
    g_player.pcm_flash_addr = 0U;
    g_player.pcm_total_bytes = 0U;
    g_player.pcm_offset_bytes = 0U;
}

void Player_Play(uint8_t track_id)
{
    const TrackMeta *t = Music_Library_GetById(track_id);
    uint8_t probe[16];
    HAL_StatusTypeDef rc;
    if (t == (const TrackMeta *)0)
    {
        return;
    }

    g_player.current_track_id = track_id;
    g_player.position_ms = 0U;
    g_player.last_tick = HAL_GetTick();
    g_player.state = PLAYER_PLAYING;
    g_player.pcm_active = 0U;
    g_player.pcm_flash_addr = 0U;
    g_player.pcm_total_bytes = 0U;
    g_player.pcm_offset_bytes = 0U;

    /* Phase-1 backend binding:
     * - TONE tracks use existing demo path
     * - PCM tracks are marked PLAYING for UI/state flow; actual PCM feed to be connected later
     */
    if (t->type == TRACK_TYPE_TONE)
    {
        Audio_Service_PlayDemoMusic();
    }
    else
    {
        if ((t->data_len == 0U) || (t->bits_per_sample != 16U) || (t->channels != 1U) || (t->sample_rate_hz != 22050U))
        {
            Debug_Log("[PLAYER] PCM meta invalid: id=%u len=%lu sr=%lu bit=%u ch=%u\r\n",
                      t->id, (unsigned long)t->data_len, (unsigned long)t->sample_rate_hz,
                      (unsigned int)t->bits_per_sample, (unsigned int)t->channels);
            g_player.state = PLAYER_STOPPED;
            return;
        }

        /* Phase-2 probe: verify PCM block is readable from W25Q128. */
        rc = W25Q128_Read(t->flash_addr, probe, sizeof(probe));
        if (rc == HAL_OK)
        {
            Debug_Log("[PLAYER] PCM probe ok: id=%u addr=0x%08lX sr=%lu bit=%u ch=%u len=%lu\r\n",
                      t->id,
                      (unsigned long)t->flash_addr,
                      (unsigned long)t->sample_rate_hz,
                      (unsigned int)t->bits_per_sample,
                      (unsigned int)t->channels,
                      (unsigned long)t->data_len);
            Debug_Log("[PLAYER] PCM head: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                      probe[0], probe[1], probe[2], probe[3], probe[4], probe[5], probe[6], probe[7]);
        }
        else
        {
            Debug_Log("[PLAYER] PCM probe failed: id=%u addr=0x%08lX\r\n",
                      t->id, (unsigned long)t->flash_addr);
            g_player.state = PLAYER_STOPPED;
            return;
        }
        Audio_Service_StartPcmStream();
        g_player.pcm_active = 1U;
        g_player.pcm_flash_addr = t->flash_addr;
        g_player.pcm_total_bytes = t->data_len;
        g_player.pcm_offset_bytes = 0U;
    }
}

void Player_Pause(void)
{
    if (g_player.state != PLAYER_PLAYING)
    {
        return;
    }
    g_player.state = PLAYER_PAUSED;
    Audio_Service_StopMusic();
}

void Player_Resume(void)
{
    const TrackMeta *t = Player_GetCurrentTrack();
    if ((g_player.state != PLAYER_PAUSED) || (t == (const TrackMeta *)0))
    {
        return;
    }
    g_player.state = PLAYER_PLAYING;
    g_player.last_tick = HAL_GetTick();
    if (t->type == TRACK_TYPE_TONE)
    {
        Audio_Service_PlayDemoMusic();
    }
}

void Player_Stop(void)
{
    g_player.state = PLAYER_STOPPED;
    g_player.position_ms = 0U;
    g_player.pcm_active = 0U;
    Audio_Service_StopMusic();
    Audio_Service_StopPcmStream();
}

void Player_Next(void)
{
    uint8_t idx;
    uint8_t n = Music_Library_GetCount();
    if (n == 0U)
    {
        return;
    }
    idx = player_find_track_index_by_id(g_player.current_track_id);
    idx = (uint8_t)((idx + 1U) % n);
    g_player.current_track_id = Music_Library_GetByIndex(idx)->id;
}

void Player_Prev(void)
{
    uint8_t idx;
    uint8_t n = Music_Library_GetCount();
    if (n == 0U)
    {
        return;
    }
    idx = player_find_track_index_by_id(g_player.current_track_id);
    idx = (idx == 0U) ? (uint8_t)(n - 1U) : (uint8_t)(idx - 1U);
    g_player.current_track_id = Music_Library_GetByIndex(idx)->id;
}

void Player_SetVolume(uint8_t percent)
{
    g_player.volume_percent = percent;
    (void)g_player.volume_percent;
}

void Player_SetRepeatMode(PlayerRepeatMode mode)
{
    g_player.repeat_mode = mode;
}

void Player_CycleRepeatMode(void)
{
    if (g_player.repeat_mode == PLAYER_REPEAT_OFF)
    {
        g_player.repeat_mode = PLAYER_REPEAT_ONE;
    }
    else if (g_player.repeat_mode == PLAYER_REPEAT_ONE)
    {
        g_player.repeat_mode = PLAYER_REPEAT_ALL;
    }
    else
    {
        g_player.repeat_mode = PLAYER_REPEAT_OFF;
    }
}

void Player_Tick(uint32_t now_tick)
{
    const TrackMeta *t = Player_GetCurrentTrack();
    uint32_t dt;
    HAL_StatusTypeDef rc;

    if (g_player.state != PLAYER_PLAYING)
    {
        g_player.last_tick = now_tick;
        return;
    }

    dt = now_tick - g_player.last_tick;
    g_player.last_tick = now_tick;
    g_player.position_ms += dt;

    if ((g_player.pcm_active != 0U) && (Audio_Service_IsPcmStreamActive() != 0U))
    {
        while ((Audio_Service_GetPcmFreeSamples() >= 128U) &&
               (g_player.pcm_offset_bytes < g_player.pcm_total_bytes))
        {
            uint32_t remain = g_player.pcm_total_bytes - g_player.pcm_offset_bytes;
            uint16_t free_samples = Audio_Service_GetPcmFreeSamples();
            uint32_t max_bytes_by_free = (uint32_t)free_samples * 2U;
            uint16_t read_len = (remain > sizeof(g_pcm_read_buf)) ? (uint16_t)sizeof(g_pcm_read_buf) : (uint16_t)remain;
            uint16_t sample_cnt;
            uint16_t pushed;
            uint16_t i;

            if (read_len < 2U)
            {
                break;
            }
            if (read_len > max_bytes_by_free)
            {
                read_len = (uint16_t)max_bytes_by_free;
            }
            read_len = (uint16_t)(read_len & 0xFFFEU);
            if (read_len == 0U)
            {
                break;
            }

            rc = W25Q128_Read(g_player.pcm_flash_addr + g_player.pcm_offset_bytes, g_pcm_read_buf, read_len);
            if (rc != HAL_OK)
            {
                Debug_Log("[PLAYER] PCM read failed at 0x%08lX\r\n",
                          (unsigned long)(g_player.pcm_flash_addr + g_player.pcm_offset_bytes));
                Player_Stop();
                return;
            }

            sample_cnt = (uint16_t)(read_len / 2U);
            for (i = 0U; i < sample_cnt; ++i)
            {
                uint16_t lo = g_pcm_read_buf[i * 2U];
                uint16_t hi = g_pcm_read_buf[i * 2U + 1U];
                g_pcm_sample_buf[i] = (int16_t)((hi << 8) | lo);
            }
            pushed = Audio_Service_PushPcmMono16(g_pcm_sample_buf, sample_cnt);
            g_player.pcm_offset_bytes += (uint32_t)pushed * 2U;
            if (pushed < sample_cnt)
            {
                break;
            }
        }
    }

    if ((t != (const TrackMeta *)0) && (t->duration_ms > 0U) && (g_player.position_ms >= t->duration_ms))
    {
        if (g_player.repeat_mode == PLAYER_REPEAT_ONE)
        {
            Player_Play(g_player.current_track_id);
        }
        else if (g_player.repeat_mode == PLAYER_REPEAT_ALL)
        {
            Player_Next();
            Player_Play(g_player.current_track_id);
        }
        else
        {
            Player_Stop();
        }
    }
}

PlayerState Player_GetState(void)
{
    return g_player.state;
}

PlayerRepeatMode Player_GetRepeatMode(void)
{
    return g_player.repeat_mode;
}

uint8_t Player_GetCurrentTrackId(void)
{
    return g_player.current_track_id;
}

uint32_t Player_GetPositionMs(void)
{
    return g_player.position_ms;
}

const TrackMeta *Player_GetCurrentTrack(void)
{
    return Music_Library_GetById(g_player.current_track_id);
}
