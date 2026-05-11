#ifndef __STORAGE_SERVICE_H__
#define __STORAGE_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct
{
    uint8_t sound_enabled;
    uint8_t volume_percent;
    uint8_t snake_difficulty;
    uint8_t brick_difficulty;
    uint8_t brick_init_lives;
    uint8_t reserved0;
    uint16_t snake_high_score;
    uint16_t brick_high_score;
} StorageRuntimeConfig;

void Storage_Service_Init(void);
void Storage_Service_Process(void);

uint8_t Storage_Service_GetSoundEnabled(void);
uint8_t Storage_Service_GetVolumePercent(void);
uint16_t Storage_Service_GetSnakeHighScore(void);
uint16_t Storage_Service_GetBrickHighScore(void);
uint8_t Storage_Service_GetSnakeDifficulty(void);
uint8_t Storage_Service_GetBrickDifficulty(void);
uint8_t Storage_Service_GetBrickInitLives(void);

void Storage_Service_SetSoundEnabled(uint8_t enabled);
void Storage_Service_SetVolumePercent(uint8_t volume_percent);
void Storage_Service_SetSnakeHighScore(uint16_t high_score);
void Storage_Service_SetBrickHighScore(uint16_t high_score);
void Storage_Service_SetSnakeDifficulty(uint8_t level);
void Storage_Service_SetBrickDifficulty(uint8_t level);
void Storage_Service_SetBrickInitLives(uint8_t lives);

#ifdef __cplusplus
}
#endif

#endif /* __STORAGE_SERVICE_H__ */
