#ifndef __GAME_IFACE_H__
#define __GAME_IFACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct
{
    const char *name;
    void (*init)(void);
    void (*enter)(void);
    void (*handle_input)(uint16_t navigation_events, uint16_t pressed_events);
    void (*update)(uint32_t now_tick);
    void (*render)(void);
    uint8_t (*consume_exit_request)(void);
    uint16_t (*get_high_score)(void);
    void (*set_high_score)(uint16_t high_score);
} GameOps;

#ifdef __cplusplus
}
#endif

#endif /* __GAME_IFACE_H__ */

