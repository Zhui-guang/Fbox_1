#ifndef __PLAYER_SERVICE_H__
#define __PLAYER_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "music_library.h"

typedef enum
{
    PLAYER_STOPPED = 0,
    PLAYER_PLAYING,
    PLAYER_PAUSED
} PlayerState;

typedef enum
{
    PLAYER_REPEAT_OFF = 0,
    PLAYER_REPEAT_ONE,
    PLAYER_REPEAT_ALL
} PlayerRepeatMode;

void Player_Init(void);
void Player_Tick(uint32_t now_tick);
void Player_Play(uint8_t track_id);
void Player_Pause(void);
void Player_Resume(void);
void Player_Stop(void);
void Player_Next(void);
void Player_Prev(void);
void Player_SetVolume(uint8_t percent);
void Player_SetRepeatMode(PlayerRepeatMode mode);
void Player_CycleRepeatMode(void);

PlayerState Player_GetState(void);
PlayerRepeatMode Player_GetRepeatMode(void);
uint8_t Player_GetCurrentTrackId(void);
uint32_t Player_GetPositionMs(void);
const TrackMeta *Player_GetCurrentTrack(void);

#ifdef __cplusplus
}
#endif

#endif /* __PLAYER_SERVICE_H__ */

