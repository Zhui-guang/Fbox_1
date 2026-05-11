#include "music_library.h"

/* Phase-1: metadata only. PCM payload loading from W25Q128 will be connected later. */
static const TrackMeta k_tracks[] = {
    {
        1U, "东方红(演示音)", 32000U, TRACK_TYPE_TONE,
        16000U, 16U, 1U, 0x00000000U,
        (const void *)0, 0U
    },
    {
        2U, "赳赳老秦",             206580U, TRACK_TYPE_PCM,
        22050U, 16U, 1U, 0x00001000U,
        (const void *)0, 9110208U
    },
    {
        3U, "开机音乐",             1069U, TRACK_TYPE_PCM,
        22050U, 16U, 1U, 0x00980000U,
        (const void *)0, 47152U
    },
    {
        4U, "失败音效",             1045U, TRACK_TYPE_PCM,
        22050U, 16U, 1U, 0x00990000U,
        (const void *)0, 46080U
    }
};

uint8_t Music_Library_GetCount(void)
{
    return (uint8_t)(sizeof(k_tracks) / sizeof(k_tracks[0]));
}

const TrackMeta *Music_Library_GetByIndex(uint8_t index)
{
    if (index >= Music_Library_GetCount())
    {
        return (const TrackMeta *)0;
    }
    return &k_tracks[index];
}

const TrackMeta *Music_Library_GetById(uint8_t id)
{
    uint8_t i;
    for (i = 0U; i < Music_Library_GetCount(); ++i)
    {
        if (k_tracks[i].id == id)
        {
            return &k_tracks[i];
        }
    }
    return (const TrackMeta *)0;
}
