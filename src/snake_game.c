#include "snake_game.h"
#include "screen.h"
#include "input_service.h"
#include "audio_service.h"
#include "player_service.h"
#include "ui_layout.h"
#include <stdio.h>

#define SNAKE_CELL_SIZE       10U
#define SNAKE_COLS            (UI_SAFE_W / SNAKE_CELL_SIZE)
#define SNAKE_ROWS            (UI_CONTENT_H / SNAKE_CELL_SIZE)
#define SNAKE_BOARD_W         (SNAKE_COLS * SNAKE_CELL_SIZE)
#define SNAKE_BOARD_H         (SNAKE_ROWS * SNAKE_CELL_SIZE)
#define SNAKE_BOARD_X         (UI_SAFE_X + ((UI_SAFE_W - SNAKE_BOARD_W) / 2U))
#define SNAKE_BOARD_Y         UI_TOP_H
#define SNAKE_MAX_LEN         (SNAKE_COLS * SNAKE_ROWS)
#define SNAKE_STEP_MS_DEFAULT 120U
#define SNAKE_STEP_MS_MIN     60U

#define COLOR_BG     0x0000U
#define COLOR_GRID   0x1082U
#define COLOR_SNAKE  0x07E0U
#define COLOR_HEAD   0xAFE5U
#define COLOR_FOOD   0xF800U
#define COLOR_TEXT   0xFFFFU
#define COLOR_ACCENT 0xFFE0U

typedef enum
{
    SNAKE_DIR_UP = 0,
    SNAKE_DIR_DOWN,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_RIGHT
} SnakeDir;

typedef enum
{
    SNAKE_STATE_RUNNING = 0,
    SNAKE_STATE_PAUSED,
    SNAKE_STATE_GAME_OVER
} SnakeState;

typedef struct
{
    uint8_t x;
    uint8_t y;
} SnakePoint;

static SnakePoint snake_body[SNAKE_MAX_LEN];
static uint16_t snake_len;
static SnakePoint food;
static SnakeDir dir_current;
static SnakeDir dir_next;
static SnakeState snake_state;
static uint16_t score;
static uint16_t high_score;
static uint32_t last_step_tick;
static uint32_t step_ms;
static uint8_t snake_difficulty = 2U; /* 1鏄?2涓?3闅?*/
static uint32_t rng_state;
static uint8_t exit_requested;
static uint8_t render_full;
static uint8_t hud_dirty;
static uint8_t delta_valid;
static uint8_t delta_clear_tail_valid;
static SnakePoint delta_old_head;
static SnakePoint delta_new_head;
static SnakePoint delta_clear_tail;
static SnakeState last_rendered_state;
static uint16_t last_rendered_score;
static uint16_t last_rendered_high_score;
static uint8_t food_dirty;

static void snake_play_fail_sound(void)
{
    if (Audio_Service_IsEnabled() == 0U)
    {
        return;
    }
    Player_Play(4U);
    if (Player_GetState() != PLAYER_PLAYING)
    {
        Audio_Service_PlayEvent(SOUND_GAME_OVER);
    }
}

static uint32_t snake_rand_next(void)
{
    rng_state = (rng_state * 1664525UL) + 1013904223UL;
    return rng_state;
}

static uint8_t snake_is_occupied(uint8_t x, uint8_t y)
{
    uint16_t i;
    for (i = 0U; i < snake_len; ++i)
    {
        if (snake_body[i].x == x && snake_body[i].y == y)
        {
            return 1U;
        }
    }
    return 0U;
}

static void snake_spawn_food(void)
{
    uint16_t tries = 0U;
    uint8_t x;
    uint8_t y;

    do
    {
        x = (uint8_t)(snake_rand_next() % SNAKE_COLS);
        y = (uint8_t)(snake_rand_next() % SNAKE_ROWS);
        ++tries;
    } while (snake_is_occupied(x, y) && tries < 2000U);

    food.x = x;
    food.y = y;
    food_dirty = 1U;
}

static void snake_reset(void)
{
    uint8_t cx = (uint8_t)(SNAKE_COLS / 2U);
    uint8_t cy = (uint8_t)(SNAKE_ROWS / 2U);

    snake_len = 4U;
    snake_body[0].x = cx;
    snake_body[0].y = cy;
    snake_body[1].x = (uint8_t)(cx - 1U);
    snake_body[1].y = cy;
    snake_body[2].x = (uint8_t)(cx - 2U);
    snake_body[2].y = cy;
    snake_body[3].x = (uint8_t)(cx - 3U);
    snake_body[3].y = cy;

    dir_current = SNAKE_DIR_RIGHT;
    dir_next = SNAKE_DIR_RIGHT;
    snake_state = SNAKE_STATE_RUNNING;
    score = 0U;
    if (snake_difficulty == 1U) step_ms = 150U;
    else if (snake_difficulty == 3U) step_ms = 95U;
    else step_ms = SNAKE_STEP_MS_DEFAULT;
    snake_spawn_food();
    render_full = 1U;
    hud_dirty = 1U;
    delta_valid = 0U;
    delta_clear_tail_valid = 0U;
    food_dirty = 1U;
}

static uint8_t snake_dir_is_opposite(SnakeDir a, SnakeDir b)
{
    if ((a == SNAKE_DIR_UP && b == SNAKE_DIR_DOWN) || (a == SNAKE_DIR_DOWN && b == SNAKE_DIR_UP))
    {
        return 1U;
    }
    if ((a == SNAKE_DIR_LEFT && b == SNAKE_DIR_RIGHT) || (a == SNAKE_DIR_RIGHT && b == SNAKE_DIR_LEFT))
    {
        return 1U;
    }
    return 0U;
}

static void snake_draw_cell(uint8_t gx, uint8_t gy, uint16_t color)
{
    uint16_t px = (uint16_t)(SNAKE_BOARD_X + gx * SNAKE_CELL_SIZE);
    uint16_t py = (uint16_t)(SNAKE_BOARD_Y + gy * SNAKE_CELL_SIZE);
    Screen_FillRect(px, py, SNAKE_CELL_SIZE, SNAKE_CELL_SIZE, color);
}

static void snake_render_hud(void)
{
    char line1[40];

    Screen_FillRect(UI_SAFE_X, 0U, UI_SAFE_W, UI_TOP_H, COLOR_BG);
    (void)snprintf(line1, sizeof(line1), "贪吃蛇 分:%u 高:%u 长:%u", score, high_score, snake_len);
    Screen_DrawTextUtf8Fallback(UI_HEADER_TEXT_X, 2U, line1, COLOR_TEXT, COLOR_BG);

    if (snake_state == SNAKE_STATE_PAUSED)
    {
        Screen_DrawTextUtf8Fallback((uint16_t)(UI_HEADER_TEXT_X + 138U), 2U, "暂停", COLOR_ACCENT, COLOR_BG);
    }
    else if (snake_state == SNAKE_STATE_GAME_OVER)
    {
        Screen_DrawTextUtf8Fallback((uint16_t)(UI_HEADER_TEXT_X + 120U), 2U, "结束", COLOR_FOOD, COLOR_BG);
        Screen_FillRect(UI_SAFE_X, UI_SCREEN_H - UI_BOTTOM_H, UI_SAFE_W, UI_BOTTOM_H, COLOR_BG);
        Screen_DrawTextUtf8Fallback(UI_FOOTER_TEXT_X, UI_SCREEN_H - UI_BOTTOM_H, "START暂停  B/MENU返回", COLOR_TEXT, COLOR_BG);
    }
    else
    {
        Screen_FillRect(UI_SAFE_X, UI_SCREEN_H - UI_BOTTOM_H, UI_SAFE_W, UI_BOTTOM_H, COLOR_BG);
        Screen_DrawTextUtf8Fallback(UI_FOOTER_TEXT_X, UI_SCREEN_H - UI_BOTTOM_H, "A重开  B/MENU返回", COLOR_TEXT, COLOR_BG);
    }
}

void Snake_Game_Init(void)
{
    rng_state = HAL_GetTick() ^ 0xA5A55A5AU;
    high_score = 0U;
    exit_requested = 0U;
    last_rendered_state = SNAKE_STATE_RUNNING;
    last_rendered_score = 0xFFFFU;
    last_rendered_high_score = 0xFFFFU;
    snake_reset();
}

void Snake_Game_Enter(void)
{
    snake_reset();
    last_step_tick = HAL_GetTick();
    exit_requested = 0U;
    render_full = 1U;
    hud_dirty = 1U;
}

void Snake_Game_HandleInput(uint16_t navigation_events, uint16_t pressed_events)
{
    SnakeDir requested = dir_next;

    if (pressed_events & (INPUT_EVENT_B | INPUT_EVENT_MENU))
    {
        exit_requested = 1U;
    }

    if (pressed_events & INPUT_EVENT_START)
    {
        if (snake_state == SNAKE_STATE_RUNNING)
        {
            snake_state = SNAKE_STATE_PAUSED;
            hud_dirty = 1U;
            Audio_Service_PlayEvent(SOUND_PAUSE);
        }
        else if (snake_state == SNAKE_STATE_PAUSED)
        {
            snake_state = SNAKE_STATE_RUNNING;
            hud_dirty = 1U;
            Audio_Service_PlayEvent(SOUND_PAUSE);
        }
    }

    if (snake_state == SNAKE_STATE_GAME_OVER)
    {
        if (pressed_events & INPUT_EVENT_A)
        {
            snake_reset();
            last_step_tick = HAL_GetTick();
            hud_dirty = 1U;
        }
        return;
    }

    if (navigation_events & INPUT_EVENT_UP)
    {
        requested = SNAKE_DIR_UP;
    }
    else if (navigation_events & INPUT_EVENT_DOWN)
    {
        requested = SNAKE_DIR_DOWN;
    }
    else if (navigation_events & INPUT_EVENT_LEFT)
    {
        requested = SNAKE_DIR_LEFT;
    }
    else if (navigation_events & INPUT_EVENT_RIGHT)
    {
        requested = SNAKE_DIR_RIGHT;
    }

    if (!snake_dir_is_opposite(dir_current, requested))
    {
        dir_next = requested;
    }
}

void Snake_Game_Update(uint32_t now_tick)
{
    SnakePoint new_head;
    SnakePoint old_head;
    SnakePoint old_tail;
    uint16_t i;
    uint8_t ate_food = 0U;

    if (snake_state != SNAKE_STATE_RUNNING)
    {
        return;
    }

    if ((now_tick - last_step_tick) < step_ms)
    {
        return;
    }
    last_step_tick = now_tick;
    dir_current = dir_next;
    old_head = snake_body[0];
    old_tail = snake_body[snake_len - 1U];
    new_head = snake_body[0];

    if (dir_current == SNAKE_DIR_UP)
    {
        if (new_head.y == 0U)
        {
            snake_state = SNAKE_STATE_GAME_OVER;
            hud_dirty = 1U;
            snake_play_fail_sound();
            return;
        }
        new_head.y--;
    }
    else if (dir_current == SNAKE_DIR_DOWN)
    {
        new_head.y++;
        if (new_head.y >= SNAKE_ROWS)
        {
            snake_state = SNAKE_STATE_GAME_OVER;
            hud_dirty = 1U;
            snake_play_fail_sound();
            return;
        }
    }
    else if (dir_current == SNAKE_DIR_LEFT)
    {
        if (new_head.x == 0U)
        {
            snake_state = SNAKE_STATE_GAME_OVER;
            hud_dirty = 1U;
            snake_play_fail_sound();
            return;
        }
        new_head.x--;
    }
    else
    {
        new_head.x++;
        if (new_head.x >= SNAKE_COLS)
        {
            snake_state = SNAKE_STATE_GAME_OVER;
            hud_dirty = 1U;
            snake_play_fail_sound();
            return;
        }
    }

    for (i = 0U; i < snake_len; ++i)
    {
        if (snake_body[i].x == new_head.x && snake_body[i].y == new_head.y)
        {
            snake_state = SNAKE_STATE_GAME_OVER;
            hud_dirty = 1U;
            snake_play_fail_sound();
            return;
        }
    }

    if (new_head.x == food.x && new_head.y == food.y)
    {
        ate_food = 1U;
    }

    for (i = snake_len; i > 0U; --i)
    {
        snake_body[i] = snake_body[i - 1U];
    }
    snake_body[0] = new_head;

    if (ate_food != 0U)
    {
        if (snake_len < SNAKE_MAX_LEN - 1U)
        {
            snake_len++;
        }
        score++;
        if (score > high_score)
        {
            high_score = score;
        }
        if (step_ms > SNAKE_STEP_MS_MIN)
        {
            step_ms -= 2U;
        }
        snake_spawn_food();
        hud_dirty = 1U;
        Audio_Service_PlayEvent(SOUND_SCORE);
    }

    delta_old_head = old_head;
    delta_new_head = new_head;
    delta_valid = 1U;
    if (ate_food == 0U)
    {
        delta_clear_tail = old_tail;
        delta_clear_tail_valid = 1U;
    }
    else
    {
        delta_clear_tail_valid = 0U;
    }
}

void Snake_Game_Render(void)
{
    uint8_t x;
    uint8_t y;
    uint16_t i;

    if (render_full != 0U)
    {
        Screen_FillRect(UI_SAFE_X, SNAKE_BOARD_Y, UI_SAFE_W, UI_CONTENT_H, COLOR_BG);

        for (y = 0U; y < SNAKE_ROWS; ++y)
        {
            for (x = 0U; x < SNAKE_COLS; ++x)
            {
                snake_draw_cell(x, y, COLOR_GRID);
            }
        }

        snake_draw_cell(food.x, food.y, COLOR_FOOD);
        food_dirty = 0U;
        for (i = 0U; i < snake_len; ++i)
        {
            snake_draw_cell(snake_body[i].x, snake_body[i].y, (i == 0U) ? COLOR_HEAD : COLOR_SNAKE);
        }
        render_full = 0U;
        delta_valid = 0U;
        delta_clear_tail_valid = 0U;
    }
    else if (delta_valid != 0U)
    {
        snake_draw_cell(delta_old_head.x, delta_old_head.y, COLOR_SNAKE);
        snake_draw_cell(delta_new_head.x, delta_new_head.y, COLOR_HEAD);
        if (delta_clear_tail_valid != 0U)
        {
            snake_draw_cell(delta_clear_tail.x, delta_clear_tail.y, COLOR_GRID);
        }
        if (food_dirty != 0U)
        {
            snake_draw_cell(food.x, food.y, COLOR_FOOD);
            food_dirty = 0U;
        }
        delta_valid = 0U;
        delta_clear_tail_valid = 0U;
    }

    if ((hud_dirty != 0U) ||
        (snake_state != last_rendered_state) ||
        (score != last_rendered_score) ||
        (high_score != last_rendered_high_score))
    {
        snake_render_hud();
        hud_dirty = 0U;
        last_rendered_state = snake_state;
        last_rendered_score = score;
        last_rendered_high_score = high_score;
    }
}

uint8_t Snake_Game_ConsumeExitRequest(void)
{
    uint8_t requested = exit_requested;
    exit_requested = 0U;
    return requested;
}

uint16_t Snake_Game_GetHighScore(void)
{
    return high_score;
}

void Snake_Game_SetHighScore(uint16_t hs)
{
    high_score = hs;
    hud_dirty = 1U;
}

const GameOps *Snake_Game_GetOps(void)
{
    static const GameOps ops = {
        .name = "贪吃蛇",
        .init = Snake_Game_Init,
        .enter = Snake_Game_Enter,
        .handle_input = Snake_Game_HandleInput,
        .update = Snake_Game_Update,
        .render = Snake_Game_Render,
        .consume_exit_request = Snake_Game_ConsumeExitRequest,
        .get_high_score = Snake_Game_GetHighScore,
        .set_high_score = Snake_Game_SetHighScore
    };
    return &ops;
}

void Snake_Game_SetDifficulty(uint8_t level)
{
    if (level < 1U) level = 1U;
    if (level > 3U) level = 3U;
    snake_difficulty = level;
}

uint8_t Snake_Game_GetDifficulty(void)
{
    return snake_difficulty;
}

