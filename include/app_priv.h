#ifndef __APP_PRIV_H__
#define __APP_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "app_state.h"
#include "input_service.h"
#include "input_config.h"
#include "game_iface.h"

#define APP_INPUT_PERIOD_MS 8U
#define APP_LED_PERIOD_MS   500U
#define APP_BOOT_TIME_MS    2200U
#define APP_PAGE_PRINT_MS   500U
#define APP_GAME_RENDER_MS  16U
#define APP_BOOT_ANIM_MS    80U

#define APP_MENU_ITEM_COUNT 6U
#define APP_SETTINGS_ITEM_COUNT 10U

typedef enum
{
    APP_GAME_SNAKE = 0,
    APP_GAME_BRICK
} AppGameId;

typedef struct
{
    AppState app_state;
    uint8_t menu_index;
    uint8_t need_render;
    uint32_t boot_start_tick;
    uint32_t last_input_tick;
    uint32_t last_led_tick;
    uint32_t last_page_print_tick;
    uint32_t last_snake_render_tick;
    uint32_t last_boot_anim_tick;
    uint8_t boot_anim_phase;
    uint8_t input_test_log_enabled;
    uint16_t debug_pressed_events;
    uint16_t debug_released_events;
    uint16_t debug_repeat_events;
    uint8_t menu_prev_index;
    uint8_t menu_dirty_only;
    uint8_t music_menu_index;
    uint8_t music_prev_index;
    uint8_t settings_index;
    uint8_t settings_prev_index;
    const GameOps *active_game;
    AppGameId active_game_id;
    uint8_t boot_static_drawn;
    uint8_t boot_prev_phase;
    AppState last_render_state;
} AppContext;

extern const char *const g_app_menu_items[APP_MENU_ITEM_COUNT];

void App_ChangeState(AppContext *ctx, AppState next_state);
void App_LatchDebugEvents(AppContext *ctx, const InputSnapshot *input);
void App_PrintInputTest(AppContext *ctx, uint32_t now);
void App_RenderCurrent(AppContext *ctx);
void App_ProcessInput(AppContext *ctx, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PRIV_H__ */
