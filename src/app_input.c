#include "app_priv.h"
#include "audio_service.h"
#include "storage_service.h"

static void app_handle_common_navigation(AppContext *ctx, uint16_t pressed_events)
{
    if ((pressed_events & INPUT_EVENT_MENU) || (pressed_events & INPUT_EVENT_B))
    {
        App_ChangeState(ctx, APP_STATE_MAIN_MENU);
    }
}

static void app_menu_enter_selected(AppContext *ctx)
{
    switch (ctx->menu_index)
    {
    case 0:
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->enter != (void (*)(void))0))
        {
            ctx->active_game->enter();
        }
        App_ChangeState(ctx, APP_STATE_SNAKE_GAME);
        break;
    case 1:
        ctx->music_menu_index = 0U;
        App_ChangeState(ctx, APP_STATE_MUSIC_PLAYER);
        break;
    case 2:
        App_ChangeState(ctx, APP_STATE_INPUT_TEST);
        break;
    case 3:
        App_ChangeState(ctx, APP_STATE_SETTINGS);
        break;
    case 4:
        App_ChangeState(ctx, APP_STATE_ABOUT);
        break;
    default:
        break;
    }
}

static void app_handle_main_menu(AppContext *ctx, uint16_t navigation_events, uint16_t pressed_events)
{
    if (navigation_events & INPUT_EVENT_UP)
    {
        ctx->menu_prev_index = ctx->menu_index;
        ctx->menu_index = (ctx->menu_index == 0U) ? (APP_MENU_ITEM_COUNT - 1U) : (uint8_t)(ctx->menu_index - 1U);
        ctx->menu_dirty_only = 1U;
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }
    else if (navigation_events & INPUT_EVENT_DOWN)
    {
        ctx->menu_prev_index = ctx->menu_index;
        ctx->menu_index = (uint8_t)((ctx->menu_index + 1U) % APP_MENU_ITEM_COUNT);
        ctx->menu_dirty_only = 1U;
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    if (pressed_events & INPUT_EVENT_A)
    {
        Audio_Service_PlayEvent(SOUND_CONFIRM);
        app_menu_enter_selected(ctx);
    }
}

static void app_handle_settings(AppContext *ctx, uint16_t navigation_events, uint16_t pressed_events)
{
    uint8_t volume = Audio_Service_GetVolumePercent();

    if (pressed_events & INPUT_EVENT_A)
    {
        uint8_t next_enabled = Audio_Service_IsEnabled() ? 0U : 1U;
        Audio_Service_SetEnabled(next_enabled);
        Storage_Service_SetSoundEnabled(next_enabled);
        if (next_enabled != 0U)
        {
            Audio_Service_PlayEvent(SOUND_CONFIRM);
        }
        ctx->need_render = 1U;
    }

    if ((navigation_events & INPUT_EVENT_RIGHT) && (volume < 10U))
    {
        volume++;
        Audio_Service_SetVolumePercent(volume);
        Storage_Service_SetVolumePercent(volume);
        if (Audio_Service_IsEnabled() != 0U)
        {
            Audio_Service_PlayEvent(SOUND_MENU_MOVE);
        }
        ctx->need_render = 1U;
    }
    else if ((navigation_events & INPUT_EVENT_LEFT) && (volume > 1U))
    {
        volume--;
        Audio_Service_SetVolumePercent(volume);
        Storage_Service_SetVolumePercent(volume);
        if (Audio_Service_IsEnabled() != 0U)
        {
            Audio_Service_PlayEvent(SOUND_MENU_MOVE);
        }
        ctx->need_render = 1U;
    }

    app_handle_common_navigation(ctx, pressed_events);
}

static void app_handle_music_player(AppContext *ctx, uint16_t navigation_events, uint16_t pressed_events)
{
    if ((navigation_events & INPUT_EVENT_UP) || (navigation_events & INPUT_EVENT_DOWN))
    {
        ctx->music_menu_index = (ctx->music_menu_index == 0U) ? 1U : 0U;
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    if (pressed_events & INPUT_EVENT_A)
    {
        if (ctx->music_menu_index == 0U)
        {
            Audio_Service_SetEnabled(1U);
            Storage_Service_SetSoundEnabled(1U);
            Audio_Service_PlayDemoMusic();
        }
        else
        {
            Audio_Service_StopMusic();
        }
        ctx->need_render = 1U;
    }

    app_handle_common_navigation(ctx, pressed_events);
}

void App_LatchDebugEvents(AppContext *ctx, const InputSnapshot *input)
{
    if (ctx->app_state == APP_STATE_INPUT_TEST)
    {
        ctx->debug_pressed_events |= input->pressed_events;
        ctx->debug_released_events |= input->released_events;
        ctx->debug_repeat_events |= input->repeat_events;
    }
}

void App_ProcessInput(AppContext *ctx, uint32_t now)
{
    const InputSnapshot *input;
    InputEventFrame frame;
    uint16_t pressed_events;
    uint16_t navigation_events;

    if ((now - ctx->last_input_tick) < APP_INPUT_PERIOD_MS)
    {
        return;
    }
    ctx->last_input_tick = now;

    Input_Service_Update();
    input = Input_Service_GetSnapshot();
    App_LatchDebugEvents(ctx, input);

    while (Input_Service_DequeueEventFrame(&frame) != 0U)
    {
        pressed_events = frame.pressed_events;
        navigation_events = frame.pressed_events | frame.repeat_events;

        switch (ctx->app_state)
        {
        case APP_STATE_MAIN_MENU:
            app_handle_main_menu(ctx, navigation_events, pressed_events);
            break;
        case APP_STATE_SNAKE_GAME:
            if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->handle_input != (void (*)(uint16_t, uint16_t))0))
            {
                ctx->active_game->handle_input(navigation_events, pressed_events);
            }
            if ((ctx->active_game != (const GameOps *)0) &&
                (ctx->active_game->consume_exit_request != (uint8_t (*)(void))0) &&
                (ctx->active_game->consume_exit_request() != 0U))
            {
                App_ChangeState(ctx, APP_STATE_MAIN_MENU);
            }
            break;
        case APP_STATE_INPUT_TEST:
            if (pressed_events & INPUT_EVENT_START)
            {
                ctx->input_test_log_enabled = (uint8_t)(ctx->input_test_log_enabled ? 0U : 1U);
                ctx->need_render = 1U;
            }
            app_handle_common_navigation(ctx, pressed_events);
            break;
        case APP_STATE_MUSIC_PLAYER:
            app_handle_music_player(ctx, navigation_events, pressed_events);
            break;
        case APP_STATE_SETTINGS:
            app_handle_settings(ctx, navigation_events, pressed_events);
            break;
        case APP_STATE_ABOUT:
            app_handle_common_navigation(ctx, pressed_events);
            break;
        default:
            break;
        }
    }
}
