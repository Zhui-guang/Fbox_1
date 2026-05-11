#include "brick_game.h"
#include "app_priv.h"
#include "audio_service.h"
#include "music_library.h"
#include "player_service.h"
#include "snake_game.h"
#include "storage_service.h"
#include "debug_log.h"

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
        ctx->active_game_id = APP_GAME_SNAKE;
        ctx->active_game = Snake_Game_GetOps();
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->enter != (void (*)(void))0))
        {
            ctx->active_game->enter();
        }
        App_ChangeState(ctx, APP_STATE_GAME);
        break;
    case 1:
        ctx->active_game_id = APP_GAME_BRICK;
        ctx->active_game = Brick_Game_GetOps();
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->enter != (void (*)(void))0))
        {
            ctx->active_game->enter();
        }
        App_ChangeState(ctx, APP_STATE_GAME);
        break;
    case 2:
        ctx->music_menu_index = 0U;
        App_ChangeState(ctx, APP_STATE_MUSIC_PLAYER);
        break;
    case 3:
        App_ChangeState(ctx, APP_STATE_INPUT_TEST);
        break;
    case 4:
        ctx->settings_prev_index = ctx->settings_index;
        App_ChangeState(ctx, APP_STATE_SETTINGS);
        break;
    case 5:
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
    uint8_t snake_diff = Storage_Service_GetSnakeDifficulty();
    uint8_t brick_diff = Storage_Service_GetBrickDifficulty();
    uint8_t brick_lives = Storage_Service_GetBrickInitLives();

    if (navigation_events & INPUT_EVENT_UP)
    {
        ctx->settings_prev_index = ctx->settings_index;
        ctx->settings_index = (ctx->settings_index == 0U) ? (APP_SETTINGS_ITEM_COUNT - 1U) : (uint8_t)(ctx->settings_index - 1U);
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }
    else if (navigation_events & INPUT_EVENT_DOWN)
    {
        ctx->settings_prev_index = ctx->settings_index;
        ctx->settings_index = (uint8_t)((ctx->settings_index + 1U) % APP_SETTINGS_ITEM_COUNT);
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    if ((pressed_events & INPUT_EVENT_A) && (ctx->settings_index == 0U))
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

    if ((ctx->settings_index == 1U) && (navigation_events & INPUT_EVENT_RIGHT) && (volume < 100U))
    {
        volume = (uint8_t)((volume >= 95U) ? 100U : (volume + 5U));
        Audio_Service_SetVolumePercent(volume);
        Storage_Service_SetVolumePercent(volume);
        if (Audio_Service_IsEnabled() != 0U)
        {
            Audio_Service_PlayEvent(SOUND_MENU_MOVE);
        }
        ctx->need_render = 1U;
    }
    else if ((ctx->settings_index == 1U) && (navigation_events & INPUT_EVENT_LEFT) && (volume > 1U))
    {
        volume = (uint8_t)((volume <= 5U) ? 1U : (volume - 5U));
        Audio_Service_SetVolumePercent(volume);
        Storage_Service_SetVolumePercent(volume);
        if (Audio_Service_IsEnabled() != 0U)
        {
            Audio_Service_PlayEvent(SOUND_MENU_MOVE);
        }
        ctx->need_render = 1U;
    }

    if ((ctx->settings_index == 2U) && (navigation_events & INPUT_EVENT_RIGHT))
    {
        snake_diff = (snake_diff >= 3U) ? 3U : (uint8_t)(snake_diff + 1U);
        Storage_Service_SetSnakeDifficulty(snake_diff);
        Snake_Game_SetDifficulty(snake_diff);
        ctx->need_render = 1U;
    }
    else if ((ctx->settings_index == 2U) && (navigation_events & INPUT_EVENT_LEFT))
    {
        snake_diff = (snake_diff <= 1U) ? 1U : (uint8_t)(snake_diff - 1U);
        Storage_Service_SetSnakeDifficulty(snake_diff);
        Snake_Game_SetDifficulty(snake_diff);
        ctx->need_render = 1U;
    }

    if ((ctx->settings_index == 3U) && (navigation_events & INPUT_EVENT_RIGHT))
    {
        brick_diff = (brick_diff >= 3U) ? 3U : (uint8_t)(brick_diff + 1U);
        Storage_Service_SetBrickDifficulty(brick_diff);
        Brick_Game_SetDifficulty(brick_diff);
        ctx->need_render = 1U;
    }
    else if ((ctx->settings_index == 3U) && (navigation_events & INPUT_EVENT_LEFT))
    {
        brick_diff = (brick_diff <= 1U) ? 1U : (uint8_t)(brick_diff - 1U);
        Storage_Service_SetBrickDifficulty(brick_diff);
        Brick_Game_SetDifficulty(brick_diff);
        ctx->need_render = 1U;
    }

    if ((ctx->settings_index == 4U) && (navigation_events & INPUT_EVENT_RIGHT))
    {
        brick_lives = (brick_lives >= 5U) ? 5U : (uint8_t)(brick_lives + 1U);
        Storage_Service_SetBrickInitLives(brick_lives);
        Brick_Game_SetInitLives(brick_lives);
        ctx->need_render = 1U;
    }
    else if ((ctx->settings_index == 4U) && (navigation_events & INPUT_EVENT_LEFT))
    {
        brick_lives = (brick_lives <= 1U) ? 1U : (uint8_t)(brick_lives - 1U);
        Storage_Service_SetBrickInitLives(brick_lives);
        Brick_Game_SetInitLives(brick_lives);
        ctx->need_render = 1U;
    }

    if (((pressed_events & INPUT_EVENT_A) && (ctx->settings_index == 5U)) || (pressed_events & INPUT_EVENT_START))
    {
        if (Audio_Service_IsEnabled() == 0U)
        {
            Audio_Service_SetEnabled(1U);
            Storage_Service_SetSoundEnabled(1U);
        }
        if (Audio_Service_GetVolumePercent() < 20U)
        {
            Audio_Service_SetVolumePercent(20U);
            Storage_Service_SetVolumePercent(20U);
        }
        Audio_Service_PlayDemoMusic();
        Audio_Service_PlayEvent(SOUND_CONFIRM);
        ctx->need_render = 1U;
    }

    if ((pressed_events & INPUT_EVENT_A) && (ctx->settings_index == 6U))
    {
        Audio_Service_StopMusic();
        ctx->need_render = 1U;
    }

    if ((pressed_events & INPUT_EVENT_A) && (ctx->settings_index == 9U))
    {
        Audio_Service_SetEnabled(0U);
        Audio_Service_SetVolumePercent(10U);
        Storage_Service_SetSoundEnabled(0U);
        Storage_Service_SetVolumePercent(10U);
        Storage_Service_SetSnakeDifficulty(2U);
        Storage_Service_SetBrickDifficulty(2U);
        Storage_Service_SetBrickInitLives(3U);
        Snake_Game_SetDifficulty(2U);
        Brick_Game_SetDifficulty(2U);
        Brick_Game_SetInitLives(3U);
        ctx->need_render = 1U;
    }

    app_handle_common_navigation(ctx, pressed_events);
}

static void app_handle_music_player(AppContext *ctx, uint16_t navigation_events, uint16_t pressed_events)
{
    uint8_t tracks = Music_Library_GetCount();
    const TrackMeta *track;

    if (tracks == 0U)
    {
        app_handle_common_navigation(ctx, pressed_events);
        return;
    }

    if (navigation_events & INPUT_EVENT_UP)
    {
        ctx->music_prev_index = ctx->music_menu_index;
        ctx->music_menu_index = (ctx->music_menu_index == 0U) ? (uint8_t)(tracks - 1U) : (uint8_t)(ctx->music_menu_index - 1U);
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }
    else if (navigation_events & INPUT_EVENT_DOWN)
    {
        ctx->music_prev_index = ctx->music_menu_index;
        ctx->music_menu_index = (uint8_t)((ctx->music_menu_index + 1U) % tracks);
        ctx->need_render = 1U;
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    if (pressed_events & INPUT_EVENT_A)
    {
        track = Music_Library_GetByIndex(ctx->music_menu_index);
        if (track != (const TrackMeta *)0)
        {
            uint8_t volume = Audio_Service_GetVolumePercent();
            Audio_Service_SetEnabled(1U);
            Storage_Service_SetSoundEnabled(1U);
            if (volume < 8U)
            {
                Audio_Service_SetVolumePercent(8U);
                Storage_Service_SetVolumePercent(8U);
            }
            if ((track->type == TRACK_TYPE_PCM) && (track->data_len == 0U))
            {
                Debug_Log("[PLAYER] 曲目 id=%u 未映射PCM数据(len=0)，已跳过\r\n", track->id);
                Audio_Service_PlayEvent(SOUND_PAUSE);
            }
            else
            {
                Player_Play(track->id);
                Audio_Service_PlayEvent(SOUND_CONFIRM);
            }
        }
        ctx->need_render = 1U;
    }

    if (pressed_events & INPUT_EVENT_START)
    {
        Player_Stop();
        ctx->need_render = 1U;
    }

    if (pressed_events & INPUT_EVENT_LEFT)
    {
        Player_Prev();
        track = Music_Library_GetById(Player_GetCurrentTrackId());
        if (track != (const TrackMeta *)0)
        {
            uint8_t i;
            for (i = 0U; i < tracks; ++i)
            {
                const TrackMeta *it = Music_Library_GetByIndex(i);
                if ((it != (const TrackMeta *)0) && (it->id == track->id))
                {
                    ctx->music_prev_index = ctx->music_menu_index;
                    ctx->music_menu_index = i;
                    break;
                }
            }
        }
        ctx->need_render = 1U;
    }

    if (pressed_events & INPUT_EVENT_RIGHT)
    {
        Player_Next();
        track = Music_Library_GetById(Player_GetCurrentTrackId());
        if (track != (const TrackMeta *)0)
        {
            uint8_t i;
            for (i = 0U; i < tracks; ++i)
            {
                const TrackMeta *it = Music_Library_GetByIndex(i);
                if ((it != (const TrackMeta *)0) && (it->id == track->id))
                {
                    ctx->music_prev_index = ctx->music_menu_index;
                    ctx->music_menu_index = i;
                    break;
                }
            }
        }
        ctx->need_render = 1U;
    }

    if (pressed_events & INPUT_EVENT_EXTRA)
    {
        Player_CycleRepeatMode();
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
        case APP_STATE_GAME:
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
