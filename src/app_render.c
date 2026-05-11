#include "app_priv.h"
#include "audio_service.h"
#include "debug_log.h"
#include "logo_badge.h"
#include "screen.h"
#include "ui_layout.h"
#include <stdio.h>

#define UI_MENU_FIRST_LINE 3U
#define UI_HINT_LINE_1     10U
#define UI_HINT_LINE_2     11U

const char *const g_app_menu_items[APP_MENU_ITEM_COUNT] = {
    "Snake Game",
    "Music Player",
    "Input Test",
    "Settings",
    "About"
};

static void app_draw_text_line(uint16_t x, uint16_t line, const char *text, uint16_t color)
{
    UI_Layout_DrawTextLine(x, line, text, color);
}

static void app_draw_logo_badge(uint16_t x, uint16_t y)
{
    Screen_DrawRGB565Bitmap(x, y, LOGO_BADGE_WIDTH, LOGO_BADGE_HEIGHT, g_logo_badge_rgb565);
}

static void app_draw_menu_item(uint8_t idx, uint8_t selected)
{
    uint16_t y = (uint16_t)((UI_MENU_FIRST_LINE + idx) * UI_LINE_HEIGHT);
    char line_buf[32];

    if (selected != 0U)
    {
        Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_HIGHLIGHT_COLOR);
        (void)snprintf(line_buf, sizeof(line_buf), "> %s", g_app_menu_items[idx]);
        Screen_DrawText(UI_MARGIN_X, y, line_buf, UI_HIGHLIGHT_TEXT, UI_HIGHLIGHT_COLOR);
    }
    else
    {
        Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_BG_COLOR);
        (void)snprintf(line_buf, sizeof(line_buf), "  %s", g_app_menu_items[idx]);
        Screen_DrawText(UI_MARGIN_X, y, line_buf, UI_FG_COLOR, UI_BG_COLOR);
    }
}

static void app_render_screen_boot(const AppContext *ctx)
{
    uint16_t cx = (uint16_t)(UI_SCREEN_W / 2U);
    uint16_t progress_w;

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("FBOX");
    app_draw_logo_badge((uint16_t)(cx - (LOGO_BADGE_WIDTH / 2U)), 66U);
    app_draw_text_line((uint16_t)(UI_MARGIN_X + 24U), 8U, "Fbox1.0 Game Console", UI_FG_COLOR);

    Screen_FillRect(UI_MARGIN_X, 238U, (uint16_t)(UI_SAFE_W - 8U), 10U, 0x1082);
    progress_w = (uint16_t)(((uint32_t)(UI_SAFE_W - 8U) * ctx->boot_anim_phase) / 20U);
    if (progress_w > 0U)
    {
        Screen_FillRect(UI_MARGIN_X, 238U, progress_w, 10U, UI_ACCENT_COLOR);
    }
    UI_Layout_DrawFooter("Booting...");
}

static void app_render_main_menu_full(const AppContext *ctx)
{
    uint8_t i;

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("MAIN MENU");
    for (i = 0U; i < APP_MENU_ITEM_COUNT; ++i)
    {
        app_draw_menu_item(i, (uint8_t)(i == ctx->menu_index));
    }
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "A:Enter", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_2, "B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("Joystick UP/DOWN to select");
}

static void app_render_main_menu_delta(const AppContext *ctx)
{
    if (ctx->menu_prev_index < APP_MENU_ITEM_COUNT)
    {
        app_draw_menu_item(ctx->menu_prev_index, 0U);
    }
    app_draw_menu_item(ctx->menu_index, 1U);
}

static uint16_t app_get_current_game_high_score(const AppContext *ctx)
{
    if ((ctx->active_game != (const GameOps *)0) &&
        (ctx->active_game->get_high_score != (uint16_t (*)(void))0))
    {
        return ctx->active_game->get_high_score();
    }
    return 0U;
}

static void app_render_screen_settings(const AppContext *ctx)
{
    char line_buf[40];

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("SETTINGS");
    (void)snprintf(line_buf, sizeof(line_buf), "Sound: %s", Audio_Service_IsEnabled() ? "ON" : "OFF");
    app_draw_text_line(UI_MARGIN_X, 3U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Volume: %u%%", Audio_Service_GetVolumePercent());
    app_draw_text_line(UI_MARGIN_X, 4U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Snake Hi: %u", app_get_current_game_high_score(ctx));
    app_draw_text_line(UI_MARGIN_X, 5U, line_buf, UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, 6U, "A: Toggle Sound", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, 7U, "L/R: Volume -/+", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, 8U, "START: Play Music", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("Sound & volume persisted");
}

static void app_render_screen_about(void)
{
    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("ABOUT");
    app_draw_text_line(UI_MARGIN_X, 3U, "Fbox_1 prototype", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, 4U, "PlatformIO + HAL", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("STM32F407 + ST7789");
}

static void app_render_screen_music_player(const AppContext *ctx)
{
    char line_buf[40];

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("MUSIC PLAYER");

    if (ctx->music_menu_index == 0U)
    {
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_HIGHLIGHT_COLOR);
        Screen_DrawText(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), "> Dong Fang Hong", UI_HIGHLIGHT_TEXT, UI_HIGHLIGHT_COLOR);
    }
    else
    {
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_BG_COLOR);
        Screen_DrawText(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), "  Dong Fang Hong", UI_FG_COLOR, UI_BG_COLOR);
    }

    if (ctx->music_menu_index == 1U)
    {
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_HIGHLIGHT_COLOR);
        Screen_DrawText(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), "> Stop", UI_HIGHLIGHT_TEXT, UI_HIGHLIGHT_COLOR);
    }
    else
    {
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_BG_COLOR);
        Screen_DrawText(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), "  Stop", UI_FG_COLOR, UI_BG_COLOR);
    }

    (void)snprintf(line_buf, sizeof(line_buf), "Playing: %s", Audio_Service_IsMusicPlaying() ? "YES" : "NO");
    app_draw_text_line(UI_MARGIN_X, 7U, line_buf, UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "A:Play/Stop  B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("Independent music page");
}

static void app_render_screen_input_test(const AppContext *ctx)
{
    char line_buf[32];
    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("INPUT TEST");
    app_draw_text_line(UI_MARGIN_X, 3U, "Move joystick/buttons", UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Log: %s (START toggle)", ctx->input_test_log_enabled ? "ON" : "OFF");
    app_draw_text_line(UI_MARGIN_X, 4U, line_buf, UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("Live input monitor");
}

static void app_update_screen_input_test_live(const InputSnapshot *input)
{
    char line_buf[40];

    Screen_FillRect(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), (uint16_t)(4U * UI_LINE_HEIGHT), UI_BG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Dir:%s", Input_DirectionToString(input->direction));
    app_draw_text_line(UI_MARGIN_X, 5U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "A:%u B:%u S:%u M:%u X:%u",
                   input->joy_sw_pressed, input->key_b_pressed, input->key_start_pressed, input->key_menu_pressed, input->key_extra_pressed);
    app_draw_text_line(UI_MARGIN_X, 6U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "X:%4u Y:%4u", input->joy_x, input->joy_y);
    app_draw_text_line(UI_MARGIN_X, 7U, line_buf, UI_FG_COLOR);
}

void App_PrintInputTest(AppContext *ctx, uint32_t now)
{
    const InputSnapshot *input = Input_Service_GetSnapshot();

    if ((now - ctx->last_page_print_tick) < APP_PAGE_PRINT_MS)
    {
        return;
    }
    ctx->last_page_print_tick = now;
    if (ctx->input_test_log_enabled != 0U)
    {
        Debug_Log("tick=%lu, dir=%s, A=%u, B=%u, S=%u, M=%u, X=%u, held=0x%04X, press=0x%04X, release=0x%04X, repeat=0x%04X\r\n",
                  now,
                  Input_DirectionToString(input->direction),
                  input->joy_sw_pressed, input->key_b_pressed, input->key_start_pressed, input->key_menu_pressed, input->key_extra_pressed,
                  input->held_events, ctx->debug_pressed_events, ctx->debug_released_events, ctx->debug_repeat_events);
    }
    app_update_screen_input_test_live(input);
    ctx->debug_pressed_events = INPUT_EVENT_NONE;
    ctx->debug_released_events = INPUT_EVENT_NONE;
    ctx->debug_repeat_events = INPUT_EVENT_NONE;
}

void App_RenderCurrent(AppContext *ctx)
{
    uint8_t i;

    switch (ctx->app_state)
    {
    case APP_STATE_BOOT_LOGO:
        Debug_Log("\r\n[FBOX] BOOT\r\n");
        Debug_Log("STM32F407 handheld game console\r\n");
        Debug_Log("Input: joystick + buttons OK\r\n");
        app_render_screen_boot(ctx);
        break;
    case APP_STATE_MAIN_MENU:
        Debug_Log("\r\n[FBOX] MAIN MENU\r\n");
        for (i = 0U; i < APP_MENU_ITEM_COUNT; ++i)
        {
            Debug_Log("%c %s\r\n", (i == ctx->menu_index) ? '>' : ' ', g_app_menu_items[i]);
        }
        Debug_Log("A=Enter, B=Back, MENU=Main\r\n");
        if (ctx->menu_dirty_only != 0U)
        {
            app_render_main_menu_delta(ctx);
        }
        else
        {
            app_render_main_menu_full(ctx);
        }
        ctx->menu_dirty_only = 0U;
        break;
    case APP_STATE_SNAKE_GAME:
        Debug_Log("\r\n[FBOX] SNAKE GAME\r\n");
        Debug_Log("START=Pause/Resume, A=Restart(after game over), B/MENU=Back\r\n");
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->render != (void (*)(void))0))
        {
            ctx->active_game->render();
        }
        break;
    case APP_STATE_INPUT_TEST:
        Debug_Log("\r\n[FBOX] INPUT TEST\r\n");
        Debug_Log("Move joystick or press buttons. B=Back, MENU=Main\r\n");
        app_render_screen_input_test(ctx);
        break;
    case APP_STATE_MUSIC_PLAYER:
        Debug_Log("\r\n[FBOX] MUSIC PLAYER\r\n");
        Debug_Log("UP/DOWN select, A confirm, B/MENU back\r\n");
        app_render_screen_music_player(ctx);
        break;
    case APP_STATE_SETTINGS:
        Debug_Log("\r\n[FBOX] SETTINGS\r\n");
        Debug_Log("Sound=%s, Volume=%u%%, SnakeHi=%u\r\n",
                  Audio_Service_IsEnabled() ? "ON" : "OFF",
                  Audio_Service_GetVolumePercent(),
                  app_get_current_game_high_score(ctx));
        Debug_Log("A=Sound, L/R=Volume, START=Music, B/MENU=Back\r\n");
        app_render_screen_settings(ctx);
        break;
    case APP_STATE_ABOUT:
        Debug_Log("\r\n[FBOX] ABOUT\r\n");
        Debug_Log("Fbox_1 STM32F407 game console prototype\r\n");
        Debug_Log("PlatformIO + STM32Cube HAL\r\n");
        Debug_Log("B=Back, MENU=Main\r\n");
        app_render_screen_about();
        break;
    default:
        break;
    }

    ctx->need_render = 0U;
}
