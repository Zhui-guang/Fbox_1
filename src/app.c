#include "app.h"
#include "app_state.h"
#include "gpio.h"
#include "input_config.h"
#include "input_service.h"
#include "audio_service.h"
#include "storage_service.h"
#include "debug_log.h"
#include "screen.h"
#include "snake_game.h"
#include "ui_layout.h"
#include "logo_badge.h"
#include <stdio.h>

/*
 * 当前 App 层是一个最小可运行的裸机状态机：
 * 1. 负责把输入服务层产生的统一事件转换为页面跳转；
 * 2. 负责周期性串口渲染菜单，暂时代替后续 LCD 渲染；
 * 3. 保留 BOOT_LOGO / MAIN_MENU / SETTINGS / ABOUT 等页面骨架，方便后续接入 ST7789。
 */
#define APP_INPUT_PERIOD_MS INPUT_SCAN_PERIOD_MS
#define APP_LED_PERIOD_MS   500U
#define APP_BOOT_TIME_MS    2200U
#define APP_PAGE_PRINT_MS   500U
#define APP_SNAKE_RENDER_MS 40U
#define APP_BOOT_ANIM_MS    80U

#define MENU_ITEM_COUNT      5U
#define UI_MENU_TITLE_LINE   1U
#define UI_MENU_FIRST_LINE   3U
#define UI_HINT_LINE_1       10U
#define UI_HINT_LINE_2       11U

static const char *const menu_items[MENU_ITEM_COUNT] = {
    "Snake Game",
    "Music Player",
    "Input Test",
    "Settings",
    "About"
};

static AppState app_state;
static uint8_t menu_index;
static uint8_t need_render;
static uint32_t boot_start_tick;
static uint32_t last_input_tick;
static uint32_t last_led_tick;
static uint32_t last_page_print_tick;
static uint32_t last_snake_render_tick;
static uint32_t last_boot_anim_tick;
static uint8_t boot_anim_phase;
static uint8_t input_test_log_enabled;

/*
 * Input_Service 的 pressed/released/repeat 是“单次扫描边沿事件”，每 20 ms 会被清除。
 * INPUT_TEST 页面只每 500 ms 打印一次，所以这里先把期间发生过的事件锁存下来，
 * 避免短按按键刚好错过打印窗口，看起来像没有被识别。
 */
static uint16_t debug_pressed_events;
static uint16_t debug_released_events;
static uint16_t debug_repeat_events;
static uint8_t menu_prev_index;
static uint8_t menu_dirty_only;
static uint8_t music_menu_index;

static void app_draw_logo_badge(uint16_t x, uint16_t y)
{
    uint16_t row;
    uint16_t col;
    uint32_t idx = 0U;

    for (row = 0U; row < LOGO_BADGE_HEIGHT; ++row)
    {
        for (col = 0U; col < LOGO_BADGE_WIDTH; ++col)
        {
            Screen_FillRect((uint16_t)(x + col), (uint16_t)(y + row), 1U, 1U, g_logo_badge_rgb565[idx++]);
        }
    }
}

static void app_draw_text_line(uint16_t x, uint16_t line, const char *text, uint16_t color)
{
    UI_Layout_DrawTextLine(x, line, text, color);
}

static void app_draw_menu_item(uint8_t idx, uint8_t selected)
{
    uint16_t y = (uint16_t)((UI_MENU_FIRST_LINE + idx) * UI_LINE_HEIGHT);
    char line_buf[32];

    if (selected != 0U)
    {
        Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_HIGHLIGHT_COLOR);
        (void)snprintf(line_buf, sizeof(line_buf), "> %s", menu_items[idx]);
        Screen_DrawText(UI_MARGIN_X, y, line_buf, UI_HIGHLIGHT_TEXT, UI_HIGHLIGHT_COLOR);
    }
    else
    {
        Screen_FillRect(UI_MARGIN_X, y, (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_BG_COLOR);
        (void)snprintf(line_buf, sizeof(line_buf), "  %s", menu_items[idx]);
        Screen_DrawText(UI_MARGIN_X, y, line_buf, UI_FG_COLOR, UI_BG_COLOR);
    }
}

static void app_render_screen_boot(void)
{
    uint16_t cx = (uint16_t)(UI_SCREEN_W / 2U);
    uint16_t y0 = 66U;
    uint16_t progress_w;

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("FBOX");

    app_draw_logo_badge((uint16_t)(cx - (LOGO_BADGE_WIDTH / 2U)), y0);

    app_draw_text_line((uint16_t)(UI_MARGIN_X + 24U), 8U, "Fbox1.0 Game Console", UI_FG_COLOR);

    Screen_FillRect(UI_MARGIN_X, 238U, (uint16_t)(UI_SAFE_W - 8U), 10U, 0x1082);
    progress_w = (uint16_t)(((uint32_t)(UI_SAFE_W - 8U) * boot_anim_phase) / 20U);
    if (progress_w > 0U)
    {
        Screen_FillRect(UI_MARGIN_X, 238U, progress_w, 10U, UI_ACCENT_COLOR);
    }
    UI_Layout_DrawFooter("Booting...");
}

static void app_render_screen_main_menu_full(void)
{
    uint8_t i;

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("MAIN MENU");

    for (i = 0U; i < MENU_ITEM_COUNT; ++i)
    {
        app_draw_menu_item(i, (uint8_t)(i == menu_index));
    }

    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "A:Enter", UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_2, "B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("Joystick UP/DOWN to select");
}

static void app_render_screen_main_menu_delta(void)
{
    if (menu_prev_index < MENU_ITEM_COUNT)
    {
        app_draw_menu_item(menu_prev_index, 0U);
    }
    app_draw_menu_item(menu_index, 1U);
}

static void app_render_screen_settings(void)
{
    char line_buf[40];

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("SETTINGS");
    (void)snprintf(line_buf, sizeof(line_buf), "Sound: %s", Audio_Service_IsEnabled() ? "ON" : "OFF");
    app_draw_text_line(UI_MARGIN_X, 3U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Volume: %u%%", Audio_Service_GetVolumePercent());
    app_draw_text_line(UI_MARGIN_X, 4U, line_buf, UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Snake Hi: %u", Snake_Game_GetHighScore());
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

static void app_render_screen_music_player(void)
{
    char line_buf[40];

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("MUSIC PLAYER");

    if (music_menu_index == 0U)
    {
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_HIGHLIGHT_COLOR);
        Screen_DrawText(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), "> Dong Fang Hong", UI_HIGHLIGHT_TEXT, UI_HIGHLIGHT_COLOR);
    }
    else
    {
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), UI_LINE_HEIGHT, UI_BG_COLOR);
        Screen_DrawText(UI_MARGIN_X, (uint16_t)(4U * UI_LINE_HEIGHT), "  Dong Fang Hong", UI_FG_COLOR, UI_BG_COLOR);
    }

    if (music_menu_index == 1U)
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

static void app_render_screen_input_test(void)
{
    char line_buf[32];
    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader("INPUT TEST");
    app_draw_text_line(UI_MARGIN_X, 3U, "Move joystick/buttons", UI_FG_COLOR);
    (void)snprintf(line_buf, sizeof(line_buf), "Log: %s (START toggle)", input_test_log_enabled ? "ON" : "OFF");
    app_draw_text_line(UI_MARGIN_X, 4U, line_buf, UI_FG_COLOR);
    app_draw_text_line(UI_MARGIN_X, UI_HINT_LINE_1, "B/MENU:Back", UI_FG_COLOR);
    UI_Layout_DrawFooter("Live input monitor");
}

static void app_update_screen_input_test_live(const InputSnapshot *input)
{
    char line_buf[40];

    /* Clear a small region for live data area only, avoid full-screen repaint each sample. */
    Screen_FillRect(UI_MARGIN_X, (uint16_t)(5U * UI_LINE_HEIGHT), (uint16_t)(UI_SAFE_W - 8U), (uint16_t)(4U * UI_LINE_HEIGHT), UI_BG_COLOR);

    (void)snprintf(line_buf, sizeof(line_buf), "Dir:%s", Input_DirectionToString(input->direction));
    app_draw_text_line(UI_MARGIN_X, 5U, line_buf, UI_FG_COLOR);

    (void)snprintf(line_buf, sizeof(line_buf), "A:%u B:%u S:%u M:%u X:%u",
                   input->joy_sw_pressed,
                   input->key_b_pressed,
                   input->key_start_pressed,
                   input->key_menu_pressed,
                   input->key_extra_pressed);
    app_draw_text_line(UI_MARGIN_X, 6U, line_buf, UI_FG_COLOR);

    (void)snprintf(line_buf, sizeof(line_buf), "X:%4u Y:%4u", input->joy_x, input->joy_y);
    app_draw_text_line(UI_MARGIN_X, 7U, line_buf, UI_FG_COLOR);
}

static void app_change_state(AppState next_state)
{
    app_state = next_state;

    /* 状态切换后强制重绘一次页面。后续替换为 LCD 时也可以复用这个脏标志思想。 */
    need_render = 1;

    /* 进入新页面时清空页面内的周期打印计时和调试边沿，避免上一页面残留。 */
    last_page_print_tick = 0;
    debug_pressed_events = INPUT_EVENT_NONE;
    debug_released_events = INPUT_EVENT_NONE;
    debug_repeat_events = INPUT_EVENT_NONE;
    menu_dirty_only = 0U;
}

static void app_render_boot(void)
{
    Debug_Log("\r\n[FBOX] BOOT\r\n");
    Debug_Log("STM32F407 handheld game console\r\n");
    Debug_Log("Input: joystick + buttons OK\r\n");
    app_render_screen_boot();
}

static void app_render_main_menu(void)
{
    uint8_t i;

    Debug_Log("\r\n[FBOX] MAIN MENU\r\n");
    for (i = 0; i < MENU_ITEM_COUNT; ++i)
    {
        Debug_Log("%c %s\r\n", (i == menu_index) ? '>' : ' ', menu_items[i]);
    }
    Debug_Log("A=Enter, B=Back, MENU=Main\r\n");
    if (menu_dirty_only != 0U)
    {
        app_render_screen_main_menu_delta();
    }
    else
    {
        app_render_screen_main_menu_full();
    }
    menu_dirty_only = 0U;
}

static void app_render_settings(void)
{
    Debug_Log("\r\n[FBOX] SETTINGS\r\n");
    Debug_Log("Sound=%s, Volume=%u%%, SnakeHi=%u\r\n",
              Audio_Service_IsEnabled() ? "ON" : "OFF",
              Audio_Service_GetVolumePercent(),
              Snake_Game_GetHighScore());
    Debug_Log("A=Sound, L/R=Volume, START=Music, B/MENU=Back\r\n");
    app_render_screen_settings();
}

static void app_render_snake(void)
{
    Debug_Log("\r\n[FBOX] SNAKE GAME\r\n");
    Debug_Log("START=Pause/Resume, A=Restart(after game over), B/MENU=Back\r\n");
    Snake_Game_Render();
}

static void app_render_about(void)
{
    Debug_Log("\r\n[FBOX] ABOUT\r\n");
    Debug_Log("Fbox_1 STM32F407 game console prototype\r\n");
    Debug_Log("PlatformIO + STM32Cube HAL\r\n");
    Debug_Log("B=Back, MENU=Main\r\n");
    app_render_screen_about();
}

static void app_render_current(void)
{
    /* 当前用串口打印模拟页面渲染；后续接屏幕后，这里可以改为 Graphics 层调用。 */
    switch (app_state)
    {
    case APP_STATE_BOOT_LOGO:
        app_render_boot();
        break;
    case APP_STATE_MAIN_MENU:
        app_render_main_menu();
        break;
    case APP_STATE_SNAKE_GAME:
        app_render_snake();
        break;
    case APP_STATE_INPUT_TEST:
        Debug_Log("\r\n[FBOX] INPUT TEST\r\n");
        Debug_Log("Move joystick or press buttons. B=Back, MENU=Main\r\n");
        app_render_screen_input_test();
        break;
    case APP_STATE_MUSIC_PLAYER:
        Debug_Log("\r\n[FBOX] MUSIC PLAYER\r\n");
        Debug_Log("UP/DOWN select, A confirm, B/MENU back\r\n");
        app_render_screen_music_player();
        break;
    case APP_STATE_SETTINGS:
        app_render_settings();
        break;
    case APP_STATE_ABOUT:
        app_render_about();
        break;
    default:
        break;
    }

    need_render = 0;
}

static void app_menu_enter_selected(void)
{
    /* 菜单索引只在本模块内部维护，外部模块不直接依赖菜单项顺序。 */
    switch (menu_index)
    {
    case 0:
        Snake_Game_Enter();
        app_change_state(APP_STATE_SNAKE_GAME);
        break;
    case 1:
        music_menu_index = 0U;
        app_change_state(APP_STATE_MUSIC_PLAYER);
        break;
    case 2:
        app_change_state(APP_STATE_INPUT_TEST);
        break;
    case 3:
        app_change_state(APP_STATE_SETTINGS);
        break;
    case 4:
        app_change_state(APP_STATE_ABOUT);
        break;
    default:
        break;
    }
}

static void app_handle_main_menu(uint16_t navigation_events, uint16_t pressed_events)
{
    /*
     * 菜单上下移动使用 pressed | repeat：
     * 短推摇杆移动一次，长按超过 INPUT_REPEAT_START_MS 后按固定间隔连发。
     */
    if (navigation_events & INPUT_EVENT_UP)
    {
        menu_prev_index = menu_index;
        menu_index = (menu_index == 0U) ? (MENU_ITEM_COUNT - 1U) : (uint8_t)(menu_index - 1U);
        menu_dirty_only = 1U;
        need_render = 1;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }
    else if (navigation_events & INPUT_EVENT_DOWN)
    {
        menu_prev_index = menu_index;
        menu_index = (uint8_t)((menu_index + 1U) % MENU_ITEM_COUNT);
        menu_dirty_only = 1U;
        need_render = 1;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    /* 确认键只吃 pressed 边沿，避免长按 A 时连续进入或重复触发。 */
    if (pressed_events & INPUT_EVENT_A)
    {
        Audio_Service_PlayEvent(SOUND_CONFIRM);
        app_menu_enter_selected();
    }
}

static void app_handle_common_navigation(uint16_t pressed_events)
{
    /* 二级页面统一使用 B 返回，MENU 回主菜单；目前两者行为相同，后续可分化。 */
    if (pressed_events & INPUT_EVENT_MENU)
    {
        app_change_state(APP_STATE_MAIN_MENU);
    }
    else if (pressed_events & INPUT_EVENT_B)
    {
        app_change_state(APP_STATE_MAIN_MENU);
    }
}

static void app_handle_settings(uint16_t navigation_events, uint16_t pressed_events)
{
    uint8_t volume;

    if (pressed_events & INPUT_EVENT_A)
    {
        uint8_t next_enabled = Audio_Service_IsEnabled() ? 0U : 1U;
        Audio_Service_SetEnabled(next_enabled);
        Storage_Service_SetSoundEnabled(next_enabled);
        if (next_enabled != 0U)
        {
            Audio_Service_PlayEvent(SOUND_CONFIRM);
        }
        need_render = 1U;
    }
    volume = Audio_Service_GetVolumePercent();
    if (navigation_events & INPUT_EVENT_RIGHT)
    {
        if (volume < 10U)
        {
            volume++;
            Audio_Service_SetVolumePercent(volume);
            Storage_Service_SetVolumePercent(volume);
            if (Audio_Service_IsEnabled() != 0U)
            {
                Audio_Service_PlayEvent(SOUND_MENU_MOVE);
            }
            need_render = 1U;
        }
    }
    else if (navigation_events & INPUT_EVENT_LEFT)
    {
        if (volume > 1U)
        {
            volume--;
            Audio_Service_SetVolumePercent(volume);
            Storage_Service_SetVolumePercent(volume);
            if (Audio_Service_IsEnabled() != 0U)
            {
                Audio_Service_PlayEvent(SOUND_MENU_MOVE);
            }
            need_render = 1U;
        }
    }

    app_handle_common_navigation(pressed_events);
}

static void app_handle_music_player(uint16_t navigation_events, uint16_t pressed_events)
{
    if (navigation_events & INPUT_EVENT_UP)
    {
        music_menu_index = (music_menu_index == 0U) ? 1U : 0U;
        need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }
    else if (navigation_events & INPUT_EVENT_DOWN)
    {
        music_menu_index = (music_menu_index == 0U) ? 1U : 0U;
        need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    if (pressed_events & INPUT_EVENT_A)
    {
        if (music_menu_index == 0U)
        {
            Audio_Service_SetEnabled(1U);
            Storage_Service_SetSoundEnabled(1U);
            Audio_Service_PlayDemoMusic();
        }
        else
        {
            Audio_Service_StopMusic();
        }
        need_render = 1U;
    }

    app_handle_common_navigation(pressed_events);
}

static void app_print_input_test(uint32_t now)
{
    const InputSnapshot *input = Input_Service_GetSnapshot();

    if ((now - last_page_print_tick) < APP_PAGE_PRINT_MS)
    {
        return;
    }

    last_page_print_tick = now;
    if (input_test_log_enabled != 0U)
    {
        Debug_Log("tick=%lu, dir=%s, A=%u, B=%u, S=%u, M=%u, X=%u, held=0x%04X, press=0x%04X, release=0x%04X, repeat=0x%04X\r\n",
                  now,
                  Input_DirectionToString(input->direction),
                  input->joy_sw_pressed,
                  input->key_b_pressed,
                  input->key_start_pressed,
                  input->key_menu_pressed,
                  input->key_extra_pressed,
                  input->held_events,
                  debug_pressed_events,
                  debug_released_events,
                  debug_repeat_events);
    }

    app_update_screen_input_test_live(input);

    /* 已经打印出的锁存事件立即清空，下一行只展示新的动作。 */
    debug_pressed_events = INPUT_EVENT_NONE;
    debug_released_events = INPUT_EVENT_NONE;
    debug_repeat_events = INPUT_EVENT_NONE;
}

static void app_latch_debug_events(const InputSnapshot *input)
{
    if (app_state == APP_STATE_INPUT_TEST)
    {
        debug_pressed_events |= input->pressed_events;
        debug_released_events |= input->released_events;
        debug_repeat_events |= input->repeat_events;
    }
}

static void app_process_input(uint32_t now)
{
    const InputSnapshot *input;
    InputEventFrame frame;
    uint16_t pressed_events;
    uint16_t navigation_events;
    uint8_t consumed_any = 0U;

    if ((now - last_input_tick) < APP_INPUT_PERIOD_MS)
    {
        return;
    }

    last_input_tick = now;

    /* 输入层只负责采样、消抖、方向判定、事件生成；App 层只消费统一事件。 */
    Input_Service_Update();
    input = Input_Service_GetSnapshot();
    app_latch_debug_events(input);

    while (Input_Service_DequeueEventFrame(&frame) != 0U)
    {
        consumed_any = 1U;
        pressed_events = frame.pressed_events;
        navigation_events = frame.pressed_events | frame.repeat_events;

        switch (app_state)
        {
        case APP_STATE_MAIN_MENU:
            app_handle_main_menu(navigation_events, pressed_events);
            break;
        case APP_STATE_SNAKE_GAME:
            Snake_Game_HandleInput(navigation_events, pressed_events);
            if (Snake_Game_ConsumeExitRequest() != 0U)
            {
                app_change_state(APP_STATE_MAIN_MENU);
            }
            break;
        case APP_STATE_INPUT_TEST:
            if (pressed_events & INPUT_EVENT_START)
            {
                input_test_log_enabled = (uint8_t)(input_test_log_enabled ? 0U : 1U);
                need_render = 1U;
            }
            app_handle_common_navigation(pressed_events);
            break;
        case APP_STATE_MUSIC_PLAYER:
            app_handle_music_player(navigation_events, pressed_events);
            break;
        case APP_STATE_SETTINGS:
            app_handle_settings(navigation_events, pressed_events);
            break;
        case APP_STATE_ABOUT:
            app_handle_common_navigation(pressed_events);
            break;
        default:
            break;
        }
    }
    if (consumed_any == 0U)
    {
        /* 队列为空时不做页面跳转，仅保留输入快照用于实时显示。 */
    }
}

void App_Init(void)
{
    app_state = APP_STATE_BOOT_LOGO;
    menu_index = 0;
    need_render = 1;
    boot_start_tick = HAL_GetTick();
    last_input_tick = 0;
    last_led_tick = 0;
    last_page_print_tick = 0;
    last_snake_render_tick = 0;
    last_boot_anim_tick = 0;
    boot_anim_phase = 0U;
    input_test_log_enabled = 0U;
    music_menu_index = 0U;
    debug_pressed_events = INPUT_EVENT_NONE;
    debug_released_events = INPUT_EVENT_NONE;
    debug_repeat_events = INPUT_EVENT_NONE;
    Audio_Service_SetVolumePercent(Storage_Service_GetVolumePercent());
    Audio_Service_SetEnabled(Storage_Service_GetSoundEnabled());
    Snake_Game_Init();
    Snake_Game_SetHighScore(Storage_Service_GetSnakeHighScore());
}

void App_Update(void)
{
    uint32_t now = HAL_GetTick();
    Audio_Service_Tick();

    /* PB2 用户 LED 作为系统心跳：能闪烁说明主循环、SysTick、时钟基本正常。 */
    if ((now - last_led_tick) >= APP_LED_PERIOD_MS)
    {
        last_led_tick = now;
        HAL_GPIO_TogglePin(USER_LED_GPIO_PORT, USER_LED_PIN);
    }

    if (need_render)
    {
        app_render_current();
    }

    /* 启动画面期间不处理输入，避免上电瞬间按键抖动直接影响菜单。 */
    if (app_state == APP_STATE_BOOT_LOGO)
    {
        if ((now - last_boot_anim_tick) >= APP_BOOT_ANIM_MS)
        {
            last_boot_anim_tick = now;
            if (boot_anim_phase < 20U)
            {
                boot_anim_phase++;
            }
            need_render = 1U;
        }
        if ((now - boot_start_tick) >= APP_BOOT_TIME_MS)
        {
            app_change_state(APP_STATE_MAIN_MENU);
        }
        return;
    }

    app_process_input(now);

    if (app_state == APP_STATE_SNAKE_GAME)
    {
        Snake_Game_Update(now);
        Storage_Service_SetSnakeHighScore(Snake_Game_GetHighScore());
        if ((now - last_snake_render_tick) >= APP_SNAKE_RENDER_MS)
        {
            last_snake_render_tick = now;
            Snake_Game_Render();
        }
    }

    if (app_state == APP_STATE_INPUT_TEST)
    {
        app_print_input_test(now);
    }

    Storage_Service_Process();
}
