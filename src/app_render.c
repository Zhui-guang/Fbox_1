#include "app_priv.h"
#include "audio_service.h"
#include "boot_text_cn.h"
#include "debug_log.h"
#include "logo_badge.h"
#include "music_library.h"
#include "player_service.h"
#include "screen.h"
#include "brick_game.h"
#include "snake_game.h"
#include "storage_service.h"
#include "ui_layout.h"
#include "ui_widgets.h"
#include <stdio.h>

#define UI_MENU_FIRST_LINE 3U
#define UI_HINT_LINE_1     10U
#define UI_HINT_LINE_2     11U

const char *const g_app_menu_items[APP_MENU_ITEM_COUNT] = {
    "贪吃蛇",
    "打砖块",
    "音乐播放器",
    "输入测试",
    "系统设置",
    "关于"
};

static void app_draw_text_line(uint16_t x, uint16_t line, const char *text, uint16_t color)
{
    UI_Layout_DrawTextLine(x, line, text, color);
}

static void app_draw_logo_badge_2x(uint16_t x, uint16_t y)
{
    uint16_t row;
    uint16_t col;
    uint16_t rowbuf[LOGO_BADGE_WIDTH * 2U];

    for (row = 0U; row < LOGO_BADGE_HEIGHT; ++row)
    {
        for (col = 0U; col < LOGO_BADGE_WIDTH; ++col)
        {
            uint16_t c = g_logo_badge_rgb565[(uint32_t)row * LOGO_BADGE_WIDTH + col];
            rowbuf[col * 2U] = c;
            rowbuf[col * 2U + 1U] = c;
        }
        Screen_DrawRGB565Bitmap(x, (uint16_t)(y + row * 2U), (uint16_t)(LOGO_BADGE_WIDTH * 2U), 1U, rowbuf);
        Screen_DrawRGB565Bitmap(x, (uint16_t)(y + row * 2U + 1U), (uint16_t)(LOGO_BADGE_WIDTH * 2U), 1U, rowbuf);
    }
}

static void app_draw_menu_item(uint8_t idx, uint8_t selected)
{
    uint16_t y = (uint16_t)((UI_MENU_FIRST_LINE + idx) * UI_LINE_HEIGHT);
    uint16_t row_w = (uint16_t)(UI_SAFE_W - 8U);
    uint16_t card_bg = (selected != 0U) ? UI_HIGHLIGHT_COLOR : 0x10A2U;
    uint16_t fg = (selected != 0U) ? UI_HIGHLIGHT_TEXT : UI_FG_COLOR;
    char line_buf[32];

    Screen_FillRect(UI_MARGIN_X, y, row_w, UI_LINE_HEIGHT, 0x0000U);
    Screen_FillRect(UI_MARGIN_X, (uint16_t)(y - 1U), row_w, UI_LINE_HEIGHT, card_bg);
    Screen_FillRect(UI_MARGIN_X, (uint16_t)(y - 1U), 3U, UI_LINE_HEIGHT, UI_ACCENT_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "%c %s", (selected != 0U) ? '>' : ' ', g_app_menu_items[idx]);
    Screen_DrawTextUtf8Fallback((uint16_t)(UI_MARGIN_X + 4U), (uint16_t)(y - 1U), line_buf, fg, card_bg);
}

static void app_render_screen_boot(AppContext *ctx)
{
    uint16_t cx = (uint16_t)(UI_SCREEN_W / 2U);
    uint16_t logo_w = (uint16_t)(LOGO_BADGE_WIDTH * 2U);
    uint16_t text_x = (uint16_t)(cx - (BOOT_TEXT_CN_WIDTH / 2U));
    uint16_t bar_w = (uint16_t)(UI_SAFE_W - 8U);
    uint16_t prev_w;
    uint16_t progress_w;

    if (ctx->boot_static_drawn == 0U)
    {
        UI_Layout_ClearScreen();
        UIW_DrawStatusBar("FBOX", "v1.0");
        app_draw_logo_badge_2x((uint16_t)(cx - (logo_w / 2U)), 42U);
        Screen_DrawRGB565Bitmap(text_x, (uint16_t)(11U * UI_LINE_HEIGHT), BOOT_TEXT_CN_WIDTH, BOOT_TEXT_CN_HEIGHT, g_boot_text_cn_rgb565);
        Screen_FillRect(UI_MARGIN_X, 238U, bar_w, 10U, 0x1082);
        UI_Layout_DrawFooter("系统启动中...");
        ctx->boot_prev_phase = 0U;
    }

    progress_w = (uint16_t)(((uint32_t)bar_w * ctx->boot_anim_phase) / 20U);
    prev_w = (uint16_t)(((uint32_t)bar_w * ctx->boot_prev_phase) / 20U);
    if (progress_w > prev_w)
    {
        Screen_FillRect((uint16_t)(UI_MARGIN_X + prev_w), 238U, (uint16_t)(progress_w - prev_w), 10U, UI_ACCENT_COLOR);
        ctx->boot_prev_phase = ctx->boot_anim_phase;
    }
}

static void app_render_main_menu_full(const AppContext *ctx)
{
    UIW_DrawList("主菜单", g_app_menu_items, APP_MENU_ITEM_COUNT, ctx->menu_index, "A确认  B/MENU返回");
}

static void app_render_main_menu_delta(const AppContext *ctx)
{
    UIW_DrawStatusBar("主菜单", (const char *)0);
    if (ctx->menu_prev_index < APP_MENU_ITEM_COUNT)
    {
        app_draw_menu_item(ctx->menu_prev_index, 0U);
    }
    app_draw_menu_item(ctx->menu_index, 1U);
}

static uint16_t app_get_snake_high_score(void)
{
    const GameOps *snake = Snake_Game_GetOps();
    if ((snake != (const GameOps *)0) && (snake->get_high_score != (uint16_t (*)(void))0))
    {
        return snake->get_high_score();
    }
    return 0U;
}

static uint16_t app_get_brick_high_score(void)
{
    const GameOps *brick = Brick_Game_GetOps();
    if ((brick != (const GameOps *)0) && (brick->get_high_score != (uint16_t (*)(void))0))
    {
        return brick->get_high_score();
    }
    return 0U;
}

static void app_render_screen_settings(const AppContext *ctx)
{
    char line_buf[48];
    uint8_t i;
    uint16_t y;
    uint16_t bg;
    uint16_t fg;
    static const char *labels[APP_SETTINGS_ITEM_COUNT] = {
        "声音开关",
        "系统音量",
        "贪吃蛇难度",
        "打砖块难度",
        "打砖块生命",
        "播放测试音乐",
        "停止音乐",
        "贪吃蛇最高分",
        "打砖块最高分",
        "恢复默认设置"
    };

    UI_Layout_ClearScreen();
    UIW_DrawStatusBar("系统设置", "配置");
    for (i = 0U; i < APP_SETTINGS_ITEM_COUNT; ++i)
    {
        y = (uint16_t)((3U + i) * UI_LINE_HEIGHT);
        bg = (i == ctx->settings_index) ? UI_HIGHLIGHT_COLOR : UI_BG_COLOR;
        fg = (i == ctx->settings_index) ? UI_HIGHLIGHT_TEXT : UI_FG_COLOR;
        Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, bg);
        if (i == 0U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %s", (i == ctx->settings_index) ? '>' : ' ', labels[i], Audio_Service_IsEnabled() ? "开" : "关");
        }
        else if (i == 1U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u%%", (i == ctx->settings_index) ? '>' : ' ', labels[i], Audio_Service_GetVolumePercent());
        }
        else if (i == 2U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (i == ctx->settings_index) ? '>' : ' ', labels[i], Storage_Service_GetSnakeDifficulty());
        }
        else if (i == 3U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (i == ctx->settings_index) ? '>' : ' ', labels[i], Storage_Service_GetBrickDifficulty());
        }
        else if (i == 4U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (i == ctx->settings_index) ? '>' : ' ', labels[i], Storage_Service_GetBrickInitLives());
        }
        else if (i == 7U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (i == ctx->settings_index) ? '>' : ' ', labels[i], app_get_snake_high_score());
        }
        else if (i == 8U)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (i == ctx->settings_index) ? '>' : ' ', labels[i], app_get_brick_high_score());
        }
        else
        {
            (void)snprintf(line_buf, sizeof(line_buf), "%c %s", (i == ctx->settings_index) ? '>' : ' ', labels[i]);
        }
        Screen_DrawTextUtf8Fallback(UI_MARGIN_X, y, line_buf, fg, bg);
    }
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU 返回", UI_FG_COLOR);
    UI_Layout_DrawFooter("上下选择 A执行 左右调节");
}

static void app_render_settings_item(const AppContext *ctx, uint8_t idx)
{
    char line_buf[48];
    uint16_t y = (uint16_t)((3U + idx) * UI_LINE_HEIGHT);
    uint16_t bg = (idx == ctx->settings_index) ? UI_HIGHLIGHT_COLOR : UI_BG_COLOR;
    uint16_t fg = (idx == ctx->settings_index) ? UI_HIGHLIGHT_TEXT : UI_FG_COLOR;
    const char *label = "";

    switch (idx)
    {
    case 0U: label = "声音开关"; break;
    case 1U: label = "系统音量"; break;
    case 2U: label = "贪吃蛇难度"; break;
    case 3U: label = "打砖块难度"; break;
    case 4U: label = "打砖块生命"; break;
    case 5U: label = "播放测试音乐"; break;
    case 6U: label = "停止音乐"; break;
    case 7U: label = "贪吃蛇最高分"; break;
    case 8U: label = "打砖块最高分"; break;
    case 9U: label = "恢复默认设置"; break;
    default: break;
    }

    Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, bg);
    if (idx == 0U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %s", (idx == ctx->settings_index) ? '>' : ' ', label, Audio_Service_IsEnabled() ? "开" : "关");
    }
    else if (idx == 1U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u%%", (idx == ctx->settings_index) ? '>' : ' ', label, Audio_Service_GetVolumePercent());
    }
    else if (idx == 2U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (idx == ctx->settings_index) ? '>' : ' ', label, Storage_Service_GetSnakeDifficulty());
    }
    else if (idx == 3U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (idx == ctx->settings_index) ? '>' : ' ', label, Storage_Service_GetBrickDifficulty());
    }
    else if (idx == 4U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (idx == ctx->settings_index) ? '>' : ' ', label, Storage_Service_GetBrickInitLives());
    }
    else if (idx == 7U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (idx == ctx->settings_index) ? '>' : ' ', label, app_get_snake_high_score());
    }
    else if (idx == 8U)
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s: %u", (idx == ctx->settings_index) ? '>' : ' ', label, app_get_brick_high_score());
    }
    else
    {
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s", (idx == ctx->settings_index) ? '>' : ' ', label);
    }
    Screen_DrawTextUtf8Fallback(UI_MARGIN_X, y, line_buf, fg, bg);
}

static void app_render_screen_settings_dynamic(const AppContext *ctx)
{
    if (ctx->settings_prev_index < APP_SETTINGS_ITEM_COUNT)
    {
        app_render_settings_item(ctx, ctx->settings_prev_index);
    }
    if (ctx->settings_index < APP_SETTINGS_ITEM_COUNT)
    {
        app_render_settings_item(ctx, ctx->settings_index);
    }

    if ((ctx->settings_index == 0U) || (ctx->settings_prev_index == 0U))
    {
        app_render_settings_item(ctx, 0U);
    }
    if ((ctx->settings_index == 1U) || (ctx->settings_prev_index == 1U))
    {
        app_render_settings_item(ctx, 1U);
    }
    if ((ctx->settings_index == 2U) || (ctx->settings_prev_index == 2U))
    {
        app_render_settings_item(ctx, 2U);
    }
    if ((ctx->settings_index == 3U) || (ctx->settings_prev_index == 3U))
    {
        app_render_settings_item(ctx, 3U);
    }
    if ((ctx->settings_index == 4U) || (ctx->settings_prev_index == 4U))
    {
        app_render_settings_item(ctx, 4U);
    }
    if ((ctx->settings_index == 7U) || (ctx->settings_prev_index == 7U))
    {
        app_render_settings_item(ctx, 7U);
    }
    if ((ctx->settings_index == 8U) || (ctx->settings_prev_index == 8U))
    {
        app_render_settings_item(ctx, 8U);
    }
}

static void app_render_screen_about(void)
{
    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("关于");
    app_draw_text_line(UI_MARGIN_X, 3U, "Fbox_1 课程设计原型", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, 4U, "PlatformIO + STM32 HAL", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU 返回", UI_FG_COLOR);
    UI_Layout_DrawFooter("STM32F407 + ST7789");
}

static void app_render_screen_music_player(const AppContext *ctx)
{
    char line_buf[64];
    const TrackMeta *current = Player_GetCurrentTrack();
    uint8_t n = Music_Library_GetCount();
    uint8_t i;

    UI_Layout_ClearScreen();
    UIW_DrawStatusBar("音乐播放器", "播放");

    for (i = 0U; i < n && i < 4U; ++i)
    {
        const TrackMeta *t = Music_Library_GetByIndex(i);
        uint16_t y = (uint16_t)((4U + i) * UI_LINE_HEIGHT);
        uint16_t bg = (i == ctx->music_menu_index) ? UI_HIGHLIGHT_COLOR : UI_BG_COLOR;
        uint16_t fg = (i == ctx->music_menu_index) ? UI_HIGHLIGHT_TEXT : UI_FG_COLOR;
        if (t == (const TrackMeta *)0)
        {
            continue;
        }
        Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, bg);
        (void)snprintf(line_buf, sizeof(line_buf), "%c %s", (i == ctx->music_menu_index) ? '>' : ' ', t->name);
        Screen_DrawTextUtf8Fallback(UI_MARGIN_X, y, line_buf, fg, bg);
    }

    if (current != (const TrackMeta *)0)
    {
        const char *st = "停止";
        if (Player_GetState() == PLAYER_PLAYING) st = "播放";
        else if (Player_GetState() == PLAYER_PAUSED) st = "暂停";
        (void)snprintf(line_buf, sizeof(line_buf), "当前: %s  [%s]", current->name, st);
        app_draw_text_line(UI_MARGIN_X, 9U, line_buf, UI_FG_COLOR);
        (void)snprintf(line_buf, sizeof(line_buf), "进度: %lu / %lu ms", (unsigned long)Player_GetPositionMs(), (unsigned long)current->duration_ms);
        app_draw_text_line(UI_MARGIN_X, 10U, line_buf, UI_FG_COLOR);
        if (current->type == TRACK_TYPE_PCM)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "PCM @0x%06lX len=%lu", (unsigned long)current->flash_addr, (unsigned long)current->data_len);
            app_draw_text_line(UI_MARGIN_X, 11U, line_buf, (current->data_len == 0U) ? UI_WARN_COLOR : UI_FG_COLOR);
            UI_Layout_DrawFooter((current->data_len == 0U) ? "PCM未映射" : "A播放 START停止 B/MENU返回");
        }
        else
        {
            app_draw_text_line(UI_MARGIN_X, 11U, "音调演示曲目", UI_FG_COLOR);
            UI_Layout_DrawFooter("A播放 START停止 B/MENU返回");
        }
    }
    else
    {
        UI_Layout_DrawFooter("上下选曲 左右切歌 X循环");
    }
}

static void app_render_music_row(uint8_t row_index, uint8_t selected, const char *label)
{
    uint16_t y;
    const TrackMeta *t = Music_Library_GetByIndex(row_index);
    (void)label;
    if (t == (const TrackMeta *)0)
    {
        return;
    }
    y = (uint16_t)((4U + row_index) * UI_LINE_HEIGHT);
    Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, (selected != 0U) ? UI_HIGHLIGHT_COLOR : UI_BG_COLOR);
    {
        char line[56];
        (void)snprintf(line, sizeof(line), "%c %s", (selected != 0U) ? '>' : ' ', t->name);
        Screen_DrawTextUtf8Fallback(UI_MARGIN_X, y, line, (selected != 0U) ? UI_HIGHLIGHT_TEXT : UI_FG_COLOR, (selected != 0U) ? UI_HIGHLIGHT_COLOR : UI_BG_COLOR);
    }
}

static void app_render_screen_music_player_dynamic(const AppContext *ctx)
{
    char line_buf[64];
    const TrackMeta *current = Player_GetCurrentTrack();

    if ((ctx->music_prev_index != ctx->music_menu_index) && (ctx->music_prev_index < Music_Library_GetCount()))
    {
        app_render_music_row(ctx->music_prev_index, 0U, "");
    }
    if (ctx->music_menu_index < Music_Library_GetCount())
    {
        app_render_music_row(ctx->music_menu_index, 1U, "");
    }

    Screen_FillRect(UI_MARGIN_X, (uint16_t)(9U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), (uint16_t)(3U * UI_LINE_HEIGHT), UI_BG_COLOR);
    if (current != (const TrackMeta *)0)
    {
        const char *st = "停止";
        if (Player_GetState() == PLAYER_PLAYING) st = "播放";
        else if (Player_GetState() == PLAYER_PAUSED) st = "暂停";
        (void)snprintf(line_buf, sizeof(line_buf), "当前: %s  [%s]", current->name, st);
        app_draw_text_line(UI_MARGIN_X, 9U, line_buf, UI_FG_COLOR);
        (void)snprintf(line_buf, sizeof(line_buf), "进度: %lu / %lu ms", (unsigned long)Player_GetPositionMs(), (unsigned long)current->duration_ms);
        app_draw_text_line(UI_MARGIN_X, 10U, line_buf, UI_FG_COLOR);
        if (current->type == TRACK_TYPE_PCM)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "PCM @0x%06lX len=%lu", (unsigned long)current->flash_addr, (unsigned long)current->data_len);
            app_draw_text_line(UI_MARGIN_X, 11U, line_buf, (current->data_len == 0U) ? UI_WARN_COLOR : UI_FG_COLOR);
            UI_Layout_DrawFooter((current->data_len == 0U) ? "PCM未映射" : "A播放 START停止 B/MENU返回");
        }
        else
        {
            app_draw_text_line(UI_MARGIN_X, 11U, "音调演示曲目", UI_FG_COLOR);
            UI_Layout_DrawFooter("A播放 START停止 B/MENU返回");
        }
    }
}

static void app_render_screen_input_test(const AppContext *ctx)
{
    char line_buf[32];
    UI_Layout_ClearScreen();
    UIW_DrawStatusBar("输入测试", "调试");
    app_draw_text_line(UI_MARGIN_X, 3U, "操作摇杆或按键", UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "日志: %s (START切换)", ctx->input_test_log_enabled ? "开" : "关");
    app_draw_text_line(UI_MARGIN_X, 4U, line_buf, UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU 返回", UI_FG_COLOR);
    UI_Layout_DrawFooter("实时输入监视");
}

static void app_render_screen_input_test_dynamic(const AppContext *ctx)
{
    char line_buf[32];
    Screen_FillRect(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_BG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "日志: %s (START切换)", ctx->input_test_log_enabled ? "开" : "关");
    app_draw_text_line(UI_MARGIN_X, 4U, line_buf, UI_FG_COLOR);
}

static void app_update_screen_input_test_live(const InputSnapshot *input)
{
    char line_buf[40];
    InputDiagnostics diag;
    Input_Service_GetDiagnostics(&diag);

    Screen_FillRect(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), (uint16_t)(5U * UI_LINE_HEIGHT), UI_BG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "方向:%s", Input_DirectionToString(input->direction));
    app_draw_text_line(UI_MARGIN_X, 5U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "A:%u B:%u S:%u M:%u X:%u",
                   input->joy_sw_pressed, input->key_b_pressed, input->key_start_pressed, input->key_menu_pressed, input->key_extra_pressed);
    app_draw_text_line(UI_MARGIN_X, 6U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "X:%4u Y:%4u", input->joy_x, input->joy_y);
    app_draw_text_line(UI_MARGIN_X, 7U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "C:%u,%u O:%lu Q:%u",
                   diag.center_x, diag.center_y,
                   (unsigned long)diag.queue_overflow_count,
                   diag.queue_peak_depth);
    app_draw_text_line(UI_MARGIN_X, 8U, line_buf, UI_FG_COLOR);
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
    uint8_t full_refresh = (ctx->last_render_state != ctx->app_state) ? 1U : 0U;

    switch (ctx->app_state)
    {
    case APP_STATE_BOOT_LOGO:
        Debug_Log("\r\n[FBOX] BOOT\r\n");
        Debug_Log("STM32F407 掌上游戏机\r\n");
        Debug_Log("Input: joystick + buttons OK\r\n");
        app_render_screen_boot(ctx);
        ((AppContext *)ctx)->boot_static_drawn = 1U;
        break;
    case APP_STATE_MAIN_MENU:
        Debug_Log("\r\n[FBOX] 主菜单\r\n");
        for (i = 0U; i < APP_MENU_ITEM_COUNT; ++i)
        {
            Debug_Log("%c %s\r\n", (i == ctx->menu_index) ? '>' : ' ', g_app_menu_items[i]);
        }
        Debug_Log("A确认, B返回, MENU主菜单\r\n");
        if ((ctx->menu_dirty_only != 0U) && (full_refresh == 0U))
        {
            app_render_main_menu_delta(ctx);
        }
        else
        {
            app_render_main_menu_full(ctx);
        }
        ctx->menu_dirty_only = 0U;
        break;
    case APP_STATE_GAME:
        Debug_Log("\r\n[FBOX] %s 游戏\r\n", (ctx->active_game != (const GameOps *)0) ? ctx->active_game->name : "未知");
        Debug_Log("START暂停继续, A重开(结束后), B/MENU返回\r\n");
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->render != (void (*)(void))0))
        {
            ctx->active_game->render();
        }
        break;
    case APP_STATE_INPUT_TEST:
        Debug_Log("\r\n[FBOX] INPUT TEST\r\n");
        Debug_Log("操作摇杆或按键，B返回，MENU主菜单\r\n");
        if (full_refresh != 0U)
        {
            app_render_screen_input_test(ctx);
        }
        else
        {
            app_render_screen_input_test_dynamic(ctx);
        }
        break;
    case APP_STATE_MUSIC_PLAYER:
        Debug_Log("\r\n[FBOX] 音乐播放器\r\n");
        Debug_Log("上下选择, A确认, B/MENU返回\r\n");
        if (full_refresh != 0U)
        {
            app_render_screen_music_player(ctx);
        }
        else
        {
            app_render_screen_music_player_dynamic(ctx);
        }
        break;
    case APP_STATE_SETTINGS:
        Debug_Log("\r\n[FBOX] 系统设置\r\n");
        Debug_Log("声音=%s, 音量=%u%%, 蛇高分=%u\r\n",
                  Audio_Service_IsEnabled() ? "开" : "关",
                  Audio_Service_GetVolumePercent(),
                  app_get_snake_high_score());
        Debug_Log("A执行, 左右调节, START音乐, B/MENU返回\r\n");
        if (full_refresh != 0U)
        {
            app_render_screen_settings(ctx);
        }
        else
        {
            app_render_screen_settings_dynamic(ctx);
        }
        break;
    case APP_STATE_ABOUT:
        Debug_Log("\r\n[FBOX] 关于\r\n");
        Debug_Log("Fbox_1 STM32F407 游戏机原型\r\n");
        Debug_Log("PlatformIO + STM32Cube HAL\r\n");
        Debug_Log("B返回, MENU主菜单\r\n");
        app_render_screen_about();
        break;
    default:
        break;
    }

    ctx->need_render = 0U;
    ctx->last_render_state = ctx->app_state;
}

