#include "brick_game.h"
#include "audio_service.h"
#include "input_service.h"
#include "player_service.h"
#include "screen.h"
#include "ui_layout.h"
#include <stdio.h>

#define BRICK_BG            0x0000U
#define BRICK_TEXT          0xFFFFU
#define BRICK_ACCENT        0xFFE0U
#define BRICK_WALL          0x1082U
#define BRICK_PADDLE        0x07FFU
#define BRICK_BALL          0xFFFFU

#define BRICK_AREA_X        UI_SAFE_X
#define BRICK_AREA_Y        UI_TOP_H
#define BRICK_AREA_W        UI_SAFE_W
#define BRICK_AREA_H        UI_CONTENT_H

#define BRICK_COLS          8U
#define BRICK_ROWS          5U
#define BRICK_CELL_W        (BRICK_AREA_W / BRICK_COLS)
#define BRICK_CELL_H        10U
#define BRICK_GAP           2U

#define PADDLE_W            36
#define PADDLE_H            6
#define BALL_SIZE           4

typedef enum
{
    BRICK_STATE_RUNNING = 0,
    BRICK_STATE_PAUSED,
    BRICK_STATE_GAME_OVER,
    BRICK_STATE_WIN
} BrickState;

static uint8_t bricks[BRICK_ROWS][BRICK_COLS];
static uint8_t bricks_left;
static int16_t paddle_x;
static int16_t paddle_y;
static int16_t ball_x;
static int16_t ball_y;
static int8_t ball_vx;
static int8_t ball_vy;
static uint16_t score;
static uint16_t high_score;
static uint8_t lives;
static uint8_t brick_init_lives = 3U;
static uint8_t brick_difficulty = 2U; /* 1鏄?2涓?3闅?*/
static BrickState state;
static uint8_t exit_requested;
static uint8_t full_redraw;
static uint8_t hud_dirty;
static uint32_t last_tick;

static void brick_play_fail_sound(void)
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

static void brick_draw_hud(void)
{
    char buf[48];
    Screen_FillRect(UI_SAFE_X, 0U, UI_SAFE_W, UI_TOP_H, BRICK_BG);
    (void)snprintf(buf, sizeof(buf), "打砖块 分:%u 高:%u 命:%u", score, high_score, lives);
    Screen_DrawTextUtf8Fallback(UI_HEADER_TEXT_X, 2U, buf, BRICK_TEXT, BRICK_BG);
    if (state == BRICK_STATE_PAUSED)
    {
        Screen_DrawTextUtf8Fallback((uint16_t)(UI_HEADER_TEXT_X + 130U), 2U, "暂停", BRICK_ACCENT, BRICK_BG);
    }
}

static void brick_draw_footer(void)
{
    Screen_FillRect(UI_SAFE_X, UI_SCREEN_H - UI_BOTTOM_H, UI_SAFE_W, UI_BOTTOM_H, BRICK_BG);
    if (state == BRICK_STATE_RUNNING)
    {
        Screen_DrawTextUtf8Fallback(UI_FOOTER_TEXT_X, UI_SCREEN_H - UI_BOTTOM_H, "START暂停  B/MENU返回", BRICK_TEXT, BRICK_BG);
    }
    else if (state == BRICK_STATE_PAUSED)
    {
        Screen_DrawTextUtf8Fallback(UI_FOOTER_TEXT_X, UI_SCREEN_H - UI_BOTTOM_H, "START继续  B/MENU返回", BRICK_TEXT, BRICK_BG);
    }
    else if (state == BRICK_STATE_WIN)
    {
        Screen_DrawTextUtf8Fallback(UI_FOOTER_TEXT_X, UI_SCREEN_H - UI_BOTTOM_H, "胜利 A重开  B/MENU返回", BRICK_TEXT, BRICK_BG);
    }
    else
    {
        Screen_DrawTextUtf8Fallback(UI_FOOTER_TEXT_X, UI_SCREEN_H - UI_BOTTOM_H, "失败 A重开  B/MENU返回", BRICK_TEXT, BRICK_BG);
    }
}

static void brick_reset_level(void)
{
    uint8_t r;
    uint8_t c;
    for (r = 0U; r < BRICK_ROWS; ++r)
    {
        for (c = 0U; c < BRICK_COLS; ++c)
        {
            bricks[r][c] = 1U;
        }
    }
    bricks_left = BRICK_ROWS * BRICK_COLS;
    paddle_x = (int16_t)(BRICK_AREA_X + (BRICK_AREA_W - PADDLE_W) / 2);
    paddle_y = (int16_t)(BRICK_AREA_Y + BRICK_AREA_H - PADDLE_H - 6);
    ball_x = (int16_t)(paddle_x + PADDLE_W / 2 - BALL_SIZE / 2);
    ball_y = (int16_t)(paddle_y - BALL_SIZE - 1);
    if (brick_difficulty == 1U) { ball_vx = 2; ball_vy = -2; }
    else if (brick_difficulty == 3U) { ball_vx = 3; ball_vy = -3; }
    else { ball_vx = 2; ball_vy = -3; }
    lives = brick_init_lives;
    score = 0U;
    state = BRICK_STATE_RUNNING;
    full_redraw = 1U;
    hud_dirty = 1U;
}

static void brick_draw_bricks(void)
{
    uint8_t r;
    uint8_t c;
    for (r = 0U; r < BRICK_ROWS; ++r)
    {
        for (c = 0U; c < BRICK_COLS; ++c)
        {
            uint16_t x = (uint16_t)(BRICK_AREA_X + c * BRICK_CELL_W + BRICK_GAP / 2U);
            uint16_t y = (uint16_t)(BRICK_AREA_Y + 4U + r * (BRICK_CELL_H + BRICK_GAP));
            uint16_t w = (uint16_t)(BRICK_CELL_W - BRICK_GAP);
            uint16_t h = BRICK_CELL_H;
            uint16_t color = (uint16_t)(0xF800U - (r * 0x200U));
            if (bricks[r][c] != 0U)
            {
                Screen_FillRect(x, y, w, h, color);
            }
            else
            {
                Screen_FillRect(x, y, w, h, BRICK_BG);
            }
        }
    }
}

static void brick_draw_objects(void)
{
    static int16_t prev_paddle_x = -1;
    static int16_t prev_ball_x = -1;
    static int16_t prev_ball_y = -1;

    if (full_redraw != 0U)
    {
        prev_paddle_x = -1;
        prev_ball_x = -1;
        prev_ball_y = -1;
    }

    if (prev_paddle_x >= 0)
    {
        Screen_FillRect((uint16_t)prev_paddle_x, (uint16_t)paddle_y, PADDLE_W, PADDLE_H, BRICK_BG);
    }
    if (prev_ball_x >= 0)
    {
        Screen_FillRect((uint16_t)prev_ball_x, (uint16_t)prev_ball_y, BALL_SIZE, BALL_SIZE, BRICK_BG);
    }

    Screen_FillRect((uint16_t)paddle_x, (uint16_t)paddle_y, PADDLE_W, PADDLE_H, BRICK_PADDLE);
    Screen_FillRect((uint16_t)ball_x, (uint16_t)ball_y, BALL_SIZE, BALL_SIZE, BRICK_BALL);

    prev_paddle_x = paddle_x;
    prev_ball_x = ball_x;
    prev_ball_y = ball_y;
}

void Brick_Game_Init(void)
{
    high_score = 0U;
    exit_requested = 0U;
    brick_reset_level();
}

void Brick_Game_Enter(void)
{
    exit_requested = 0U;
    brick_reset_level();
    last_tick = HAL_GetTick();
}

void Brick_Game_HandleInput(uint16_t navigation_events, uint16_t pressed_events)
{
    if (pressed_events & (INPUT_EVENT_B | INPUT_EVENT_MENU))
    {
        exit_requested = 1U;
        return;
    }

    if (pressed_events & INPUT_EVENT_START)
    {
        if (state == BRICK_STATE_RUNNING)
        {
            state = BRICK_STATE_PAUSED;
            hud_dirty = 1U;
            Audio_Service_PlayEvent(SOUND_PAUSE);
        }
        else if (state == BRICK_STATE_PAUSED)
        {
            state = BRICK_STATE_RUNNING;
            hud_dirty = 1U;
            Audio_Service_PlayEvent(SOUND_PAUSE);
        }
    }

    if ((state == BRICK_STATE_GAME_OVER) || (state == BRICK_STATE_WIN))
    {
        if (pressed_events & INPUT_EVENT_A)
        {
            brick_reset_level();
            last_tick = HAL_GetTick();
        }
        return;
    }

    (void)navigation_events;
}

void Brick_Game_Update(uint32_t now_tick)
{
    const InputSnapshot *snap = Input_Service_GetSnapshot();
    int16_t next_x;
    int16_t next_y;
    uint8_t r;
    uint8_t c;

    if (state != BRICK_STATE_RUNNING)
    {
        return;
    }
    if ((now_tick - last_tick) < 16U)
    {
        return;
    }
    last_tick = now_tick;

    if ((snap->held_events & INPUT_EVENT_LEFT) && (paddle_x > BRICK_AREA_X + 2))
    {
        paddle_x -= 5;
    }
    else if ((snap->held_events & INPUT_EVENT_RIGHT) && (paddle_x + PADDLE_W < BRICK_AREA_X + BRICK_AREA_W - 2))
    {
        paddle_x += 5;
    }

    next_x = (int16_t)(ball_x + ball_vx);
    next_y = (int16_t)(ball_y + ball_vy);

    if ((next_x <= BRICK_AREA_X) || (next_x + BALL_SIZE >= BRICK_AREA_X + BRICK_AREA_W))
    {
        ball_vx = (int8_t)(-ball_vx);
        next_x = (int16_t)(ball_x + ball_vx);
    }
    if (next_y <= BRICK_AREA_Y)
    {
        ball_vy = (int8_t)(-ball_vy);
        next_y = (int16_t)(ball_y + ball_vy);
    }

    if ((next_y + BALL_SIZE >= paddle_y) &&
        (next_y <= paddle_y + PADDLE_H) &&
        (next_x + BALL_SIZE >= paddle_x) &&
        (next_x <= paddle_x + PADDLE_W))
    {
        ball_vy = (int8_t)(-ball_vy);
        next_y = (int16_t)(ball_y + ball_vy);
        Audio_Service_PlayEvent(SOUND_MENU_MOVE);
    }

    for (r = 0U; r < BRICK_ROWS; ++r)
    {
        for (c = 0U; c < BRICK_COLS; ++c)
        {
            uint16_t bx;
            uint16_t by;
            uint16_t bw;
            uint16_t bh;
            if (bricks[r][c] == 0U)
            {
                continue;
            }
            bx = (uint16_t)(BRICK_AREA_X + c * BRICK_CELL_W + BRICK_GAP / 2U);
            by = (uint16_t)(BRICK_AREA_Y + 4U + r * (BRICK_CELL_H + BRICK_GAP));
            bw = (uint16_t)(BRICK_CELL_W - BRICK_GAP);
            bh = BRICK_CELL_H;

            if ((next_x + BALL_SIZE >= bx) && (next_x <= (int16_t)(bx + bw)) &&
                (next_y + BALL_SIZE >= by) && (next_y <= (int16_t)(by + bh)))
            {
                bricks[r][c] = 0U;
                bricks_left--;
                score++;
                if (score > high_score)
                {
                    high_score = score;
                }
                ball_vy = (int8_t)(-ball_vy);
                hud_dirty = 1U;
                Audio_Service_PlayEvent(SOUND_SCORE);
                goto brick_hit_done;
            }
        }
    }
brick_hit_done:

    ball_x = (int16_t)(ball_x + ball_vx);
    ball_y = (int16_t)(ball_y + ball_vy);

    if (ball_y > (int16_t)(BRICK_AREA_Y + BRICK_AREA_H))
    {
        if (lives > 0U)
        {
            lives--;
        }
        hud_dirty = 1U;
        Audio_Service_PlayEvent(SOUND_HIT);
        if (lives == 0U)
        {
            state = BRICK_STATE_GAME_OVER;
            brick_play_fail_sound();
            return;
        }
        ball_x = (int16_t)(paddle_x + PADDLE_W / 2 - BALL_SIZE / 2);
        ball_y = (int16_t)(paddle_y - BALL_SIZE - 1);
        if (brick_difficulty == 1U) { ball_vx = 2; ball_vy = -2; }
        else if (brick_difficulty == 3U) { ball_vx = 3; ball_vy = -3; }
        else { ball_vx = 2; ball_vy = -3; }
    }

    if (bricks_left == 0U)
    {
        state = BRICK_STATE_WIN;
        hud_dirty = 1U;
        Audio_Service_PlayEvent(SOUND_GAME_OVER);
    }
}

void Brick_Game_Render(void)
{
    if (full_redraw != 0U)
    {
        Screen_FillRect(BRICK_AREA_X, BRICK_AREA_Y, BRICK_AREA_W, BRICK_AREA_H, BRICK_BG);
        Screen_FillRect(BRICK_AREA_X, BRICK_AREA_Y, BRICK_AREA_W, 1U, BRICK_WALL);
        brick_draw_bricks();
        full_redraw = 0U;
        hud_dirty = 1U;
    }

    brick_draw_objects();

    if (hud_dirty != 0U)
    {
        brick_draw_hud();
        brick_draw_footer();
        hud_dirty = 0U;
    }
}

uint8_t Brick_Game_ConsumeExitRequest(void)
{
    uint8_t r = exit_requested;
    exit_requested = 0U;
    return r;
}

uint16_t Brick_Game_GetHighScore(void)
{
    return high_score;
}

void Brick_Game_SetHighScore(uint16_t hs)
{
    high_score = hs;
}

const GameOps *Brick_Game_GetOps(void)
{
    static const GameOps ops = {
        .name = "打砖块",
        .init = Brick_Game_Init,
        .enter = Brick_Game_Enter,
        .handle_input = Brick_Game_HandleInput,
        .update = Brick_Game_Update,
        .render = Brick_Game_Render,
        .consume_exit_request = Brick_Game_ConsumeExitRequest,
        .get_high_score = Brick_Game_GetHighScore,
        .set_high_score = Brick_Game_SetHighScore
    };
    return &ops;
}

void Brick_Game_SetDifficulty(uint8_t level)
{
    if (level < 1U) level = 1U;
    if (level > 3U) level = 3U;
    brick_difficulty = level;
}

uint8_t Brick_Game_GetDifficulty(void)
{
    return brick_difficulty;
}

void Brick_Game_SetInitLives(uint8_t l)
{
    if (l < 1U) l = 1U;
    if (l > 5U) l = 5U;
    brick_init_lives = l;
}

uint8_t Brick_Game_GetInitLives(void)
{
    return brick_init_lives;
}

