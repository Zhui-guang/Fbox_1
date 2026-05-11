#ifndef __AUDIO_SERVICE_H__
#define __AUDIO_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
    SOUND_NONE = 0,
    SOUND_MENU_MOVE,
    SOUND_CONFIRM,
    SOUND_SCORE,
    SOUND_HIT,
    SOUND_PAUSE,
    SOUND_GAME_OVER
} SoundEvent;

void Audio_Service_Init(void);
void Audio_Service_Tick(void);
void Audio_PostEvent(SoundEvent event_id);
void Audio_Service_PlayEvent(SoundEvent event_id);
void Audio_Service_PlayDemoMusic(void);
uint8_t Audio_Service_IsMusicPlaying(void);
void Audio_Service_StopMusic(void);
void Audio_Service_SetEnabled(uint8_t enabled);
uint8_t Audio_Service_IsEnabled(void);
void Audio_Service_SetVolumePercent(uint8_t volume_percent);
uint8_t Audio_Service_GetVolumePercent(void);
uint8_t Audio_Service_IsReady(void);
void Audio_Service_StartPcmStream(void);
void Audio_Service_StopPcmStream(void);
uint16_t Audio_Service_PushPcmMono16(const int16_t *samples, uint16_t count);
uint16_t Audio_Service_GetPcmFreeSamples(void);
uint8_t Audio_Service_IsPcmStreamActive(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_SERVICE_H__ */
