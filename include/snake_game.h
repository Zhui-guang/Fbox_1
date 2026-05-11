#ifndef __SNAKE_GAME_H__
#define __SNAKE_GAME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "game_iface.h"

void Snake_Game_Init(void);
void Snake_Game_Enter(void);
void Snake_Game_HandleInput(uint16_t navigation_events, uint16_t pressed_events);
void Snake_Game_Update(uint32_t now_tick);
void Snake_Game_Render(void);
uint8_t Snake_Game_ConsumeExitRequest(void);
uint16_t Snake_Game_GetHighScore(void);
void Snake_Game_SetHighScore(uint16_t high_score);
void Snake_Game_SetDifficulty(uint8_t level);
uint8_t Snake_Game_GetDifficulty(void);
const GameOps *Snake_Game_GetOps(void);

#ifdef __cplusplus
}
#endif

#endif /* __SNAKE_GAME_H__ */
