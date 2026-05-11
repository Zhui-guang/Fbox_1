#include "app.h"
#include "app_priv.h"
#include "audio_service.h"
#include "brick_game.h"
#include "gpio.h"
#include "player_service.h"
#include "snake_game.h"
#include "storage_service.h"
#include "snake_game.h"
#include "brick_game.h"

static AppContext g_app_ctx;

void App_ChangeState(AppContext *ctx, AppState next_state)
{
    ctx->app_state = next_state;
    ctx->need_render = 1U;
    ctx->last_page_print_tick = 0U;
    ctx->debug_pressed_events = INPUT_EVENT_NONE;
    ctx->debug_released_events = INPUT_EVENT_NONE;
    ctx->debug_repeat_events = INPUT_EVENT_NONE;
    ctx->menu_dirty_only = 0U;
}

void App_Init(void)
{
    AppContext *ctx = &g_app_ctx;

    ctx->app_state = APP_STATE_BOOT_LOGO;
    ctx->menu_index = 0U;
    ctx->need_render = 1U;
    ctx->boot_start_tick = HAL_GetTick();
    ctx->last_input_tick = 0U;
    ctx->last_led_tick = 0U;
    ctx->last_page_print_tick = 0U;
    ctx->last_snake_render_tick = 0U;
    ctx->last_boot_anim_tick = 0U;
    ctx->boot_anim_phase = 0U;
    ctx->input_test_log_enabled = 0U;
    ctx->debug_pressed_events = INPUT_EVENT_NONE;
    ctx->debug_released_events = INPUT_EVENT_NONE;
    ctx->debug_repeat_events = INPUT_EVENT_NONE;
    ctx->menu_prev_index = 0U;
    ctx->menu_dirty_only = 0U;
    ctx->music_menu_index = 0U;
    ctx->music_prev_index = 0U;
    ctx->settings_index = 0U;
    ctx->settings_prev_index = 0U;
    ctx->active_game_id = APP_GAME_SNAKE;
    ctx->active_game = Snake_Game_GetOps();
    ctx->boot_static_drawn = 0U;
    ctx->boot_prev_phase = 0U;
    ctx->last_render_state = APP_STATE_ABOUT;

    Audio_Service_SetVolumePercent(Storage_Service_GetVolumePercent());
    Audio_Service_SetEnabled(Storage_Service_GetSoundEnabled());
    Snake_Game_SetDifficulty(Storage_Service_GetSnakeDifficulty());
    Brick_Game_SetDifficulty(Storage_Service_GetBrickDifficulty());
    Brick_Game_SetInitLives(Storage_Service_GetBrickInitLives());
    Player_Init();
    if (Audio_Service_IsEnabled() != 0U)
    {
        /* 开机时自动播放开机音乐（PCM: 0x00980000）。 */
        Player_Play(3U);
    }
    if (ctx->active_game != (const GameOps *)0)
    {
        if (ctx->active_game->init != (void (*)(void))0)
        {
            ctx->active_game->init();
        }
        if (ctx->active_game_id == APP_GAME_SNAKE)
        {
            ctx->active_game->set_high_score(Storage_Service_GetSnakeHighScore());
        }
    }
    {
        const GameOps *brick = Brick_Game_GetOps();
        if ((brick != (const GameOps *)0) && (brick->init != (void (*)(void))0))
        {
            brick->init();
        }
        if ((brick != (const GameOps *)0) && (brick->set_high_score != (void (*)(uint16_t))0))
        {
            brick->set_high_score(Storage_Service_GetBrickHighScore());
        }
    }
}

void App_Update(void)
{
    uint32_t now = HAL_GetTick();
    AppContext *ctx = &g_app_ctx;

    Audio_Service_Tick();
    Player_Tick(now);

    if ((now - ctx->last_led_tick) >= APP_LED_PERIOD_MS)
    {
        ctx->last_led_tick = now;
        HAL_GPIO_TogglePin(USER_LED_GPIO_PORT, USER_LED_PIN);
    }

    if (ctx->need_render != 0U)
    {
        App_RenderCurrent(ctx);
    }

    if (ctx->app_state == APP_STATE_BOOT_LOGO)
    {
        if ((now - ctx->last_boot_anim_tick) >= APP_BOOT_ANIM_MS)
        {
            ctx->last_boot_anim_tick = now;
            if (ctx->boot_anim_phase < 20U)
            {
                ctx->boot_anim_phase++;
            }
            ctx->need_render = 1U;
        }
        if ((now - ctx->boot_start_tick) >= APP_BOOT_TIME_MS)
        {
            App_ChangeState(ctx, APP_STATE_MAIN_MENU);
        }
        return;
    }

    App_ProcessInput(ctx, now);

    if (ctx->app_state == APP_STATE_GAME)
    {
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->update != (void (*)(uint32_t))0))
        {
            ctx->active_game->update(now);
        }
        if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->get_high_score != (uint16_t (*)(void))0))
        {
            if (ctx->active_game_id == APP_GAME_SNAKE)
            {
                Storage_Service_SetSnakeHighScore(ctx->active_game->get_high_score());
            }
            else
            {
                Storage_Service_SetBrickHighScore(ctx->active_game->get_high_score());
            }
        }
        if ((now - ctx->last_snake_render_tick) >= APP_GAME_RENDER_MS)
        {
            ctx->last_snake_render_tick = now;
            if ((ctx->active_game != (const GameOps *)0) && (ctx->active_game->render != (void (*)(void))0))
            {
                ctx->active_game->render();
            }
        }
    }

    if (ctx->app_state == APP_STATE_INPUT_TEST)
    {
        App_PrintInputTest(ctx, now);
    }

    Storage_Service_Process();
}
