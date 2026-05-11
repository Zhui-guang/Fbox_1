#ifndef __MUSIC_LIBRARY_H__
#define __MUSIC_LIBRARY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
    TRACK_TYPE_TONE = 0,
    TRACK_TYPE_PCM
} TrackType;

typedef struct
{
    uint8_t id;
    const char *name;
    uint32_t duration_ms;
    TrackType type;
    uint32_t sample_rate_hz;
    uint8_t bits_per_sample;
    uint8_t channels;
    uint32_t flash_addr;
    const void *data_ptr;
    uint32_t data_len;
} TrackMeta;

uint8_t Music_Library_GetCount(void);
const TrackMeta *Music_Library_GetByIndex(uint8_t index);
const TrackMeta *Music_Library_GetById(uint8_t id);

#ifdef __cplusplus
}
#endif

#endif /* __MUSIC_LIBRARY_H__ */
