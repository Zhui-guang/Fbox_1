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
    uint16_t reserved;
    uint16_t snake_high_score;
} StorageRuntimeConfig;

void Storage_Service_Init(void);
void Storage_Service_Process(void);

uint8_t Storage_Service_GetSoundEnabled(void);
uint8_t Storage_Service_GetVolumePercent(void);
uint16_t Storage_Service_GetSnakeHighScore(void);

void Storage_Service_SetSoundEnabled(uint8_t enabled);
void Storage_Service_SetVolumePercent(uint8_t volume_percent);
void Storage_Service_SetSnakeHighScore(uint16_t high_score);

#ifdef __cplusplus
}
#endif

#endif /* __STORAGE_SERVICE_H__ */
