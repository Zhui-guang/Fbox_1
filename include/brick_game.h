#ifndef __BRICK_GAME_H__
#define __BRICK_GAME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "game_iface.h"

const GameOps *Brick_Game_GetOps(void);
void Brick_Game_SetDifficulty(uint8_t level);
uint8_t Brick_Game_GetDifficulty(void);
void Brick_Game_SetInitLives(uint8_t lives);
uint8_t Brick_Game_GetInitLives(void);

#ifdef __cplusplus
}
#endif

#endif /* __BRICK_GAME_H__ */
