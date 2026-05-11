#include "input_service.h"
#include "input_config.h"
#include "adc.h"
#include "keys.h"

#define INPUT_DIRECTION_EVENTS_MASK \
    (INPUT_EVENT_UP | INPUT_EVENT_DOWN | INPUT_EVENT_LEFT | INPUT_EVENT_RIGHT)

typedef struct
{
    uint8_t stable_state;
    uint8_t last_raw_pressed;
    uint8_t debounce_count;
} DebounceState;

static InputSnapshot input_snapshot;
static uint16_t previous_held_events;
static uint16_t repeat_reference_events;
static uint32_t repeat_start_tick;
static uint32_t repeat_last_tick;
static DebounceState joy_sw_debounce;
static DebounceState key_b_debounce;
static DebounceState key_start_debounce;
static DebounceState key_menu_debounce;
static DebounceState key_extra_debounce;

#define INPUT_EVENT_QUEUE_SIZE 16U
static InputEventFrame event_queue[INPUT_EVENT_QUEUE_SIZE];
static uint8_t event_queue_head;
static uint8_t event_queue_tail;
static uint8_t event_queue_count;

static void enqueue_event_frame(uint16_t pressed_events, uint16_t released_events, uint16_t repeat_events)
{
    InputEventFrame *slot;

    if ((pressed_events == INPUT_EVENT_NONE) &&
        (released_events == INPUT_EVENT_NONE) &&
        (repeat_events == INPUT_EVENT_NONE))
    {
        return;
    }

    if (event_queue_count >= INPUT_EVENT_QUEUE_SIZE)
    {
        /* 队列满时丢弃最旧事件，保证最新交互优先。 */
        event_queue_head = (uint8_t)((event_queue_head + 1U) % INPUT_EVENT_QUEUE_SIZE);
        event_queue_count--;
    }

    slot = &event_queue[event_queue_tail];
    slot->pressed_events = pressed_events;
    slot->released_events = released_events;
    slot->repeat_events = repeat_events;
    event_queue_tail = (uint8_t)((event_queue_tail + 1U) % INPUT_EVENT_QUEUE_SIZE);
    event_queue_count++;
}

static void debounce_init(DebounceState *state)
{
    state->stable_state = 0;
    state->last_raw_pressed = 0;
    state->debounce_count = 0;
}

static uint8_t debounce_update(DebounceState *state, uint8_t raw_pressed)
{
    /* 去抖策略：连续 INPUT_KEY_DEBOUNCE_COUNT 次采样相同，才更新稳定状态。 */
    if (raw_pressed == state->last_raw_pressed)
    {
        if (state->debounce_count < INPUT_KEY_DEBOUNCE_COUNT)
        {
            state->debounce_count++;
        }
    }
    else
    {
        state->last_raw_pressed = raw_pressed;
        state->debounce_count = 0;
    }

    if (state->debounce_count >= INPUT_KEY_DEBOUNCE_COUNT)
    {
        state->stable_state = state->last_raw_pressed;
    }

    return state->stable_state;
}

static uint8_t direction_still_active(InputDirection direction, uint16_t x, uint16_t y)
{
    /* 方向迟滞：已经进入某方向后，必须回到退出阈值以内才回 CENTER。
     * 这样可减少摇杆边缘抖动造成的方向反复切换。 */
    switch (direction)
    {
    case INPUT_DIR_LEFT:
        return x < JOY_X_LEFT_EXIT_THRESHOLD;
    case INPUT_DIR_RIGHT:
        return x > JOY_X_RIGHT_EXIT_THRESHOLD;
    case INPUT_DIR_DOWN:
        return y < JOY_Y_DOWN_EXIT_THRESHOLD;
    case INPUT_DIR_UP:
        return y > JOY_Y_UP_EXIT_THRESHOLD;
    default:
        return 0;
    }
}

static InputDirection joystick_direction_from_adc(uint16_t x, uint16_t y)
{
    uint16_t x_delta = 0;
    uint16_t y_delta = 0;

    if (direction_still_active(input_snapshot.direction, x, y))
    {
        return input_snapshot.direction;
    }

    if (x < JOY_X_LEFT_ENTER_THRESHOLD)
    {
        x_delta = JOY_X_LEFT_ENTER_THRESHOLD - x;
    }
    else if (x > JOY_X_RIGHT_ENTER_THRESHOLD)
    {
        x_delta = x - JOY_X_RIGHT_ENTER_THRESHOLD;
    }

    if (y < JOY_Y_DOWN_ENTER_THRESHOLD)
    {
        y_delta = JOY_Y_DOWN_ENTER_THRESHOLD - y;
    }
    else if (y > JOY_Y_UP_ENTER_THRESHOLD)
    {
        y_delta = y - JOY_Y_UP_ENTER_THRESHOLD;
    }

    if (x_delta == 0 && y_delta == 0)
    {
        return INPUT_DIR_CENTER;
    }

    /* 当摇杆处于斜向时，选偏移量更大的轴作为主方向。
     * 菜单和贪吃蛇一般只需要四方向，这样可避免同时触发两个方向。 */
    if (x_delta > y_delta)
    {
        InputDirection x_dir = (x < JOY_X_LEFT_ENTER_THRESHOLD) ? INPUT_DIR_LEFT : INPUT_DIR_RIGHT;
#if JOY_X_INVERT_DIRECTION
        x_dir = (x_dir == INPUT_DIR_LEFT) ? INPUT_DIR_RIGHT : INPUT_DIR_LEFT;
#endif
        return x_dir;
    }

    {
        InputDirection y_dir = (y > JOY_Y_UP_ENTER_THRESHOLD) ? INPUT_DIR_UP : INPUT_DIR_DOWN;
#if JOY_Y_INVERT_DIRECTION
        y_dir = (y_dir == INPUT_DIR_UP) ? INPUT_DIR_DOWN : INPUT_DIR_UP;
#endif
        return y_dir;
    }
}

static uint16_t events_from_snapshot(const InputSnapshot *snapshot)
{
    uint16_t events = INPUT_EVENT_NONE;

    switch (snapshot->direction)
    {
    case INPUT_DIR_UP:
        events |= INPUT_EVENT_UP;
        break;
    case INPUT_DIR_DOWN:
        events |= INPUT_EVENT_DOWN;
        break;
    case INPUT_DIR_LEFT:
        events |= INPUT_EVENT_LEFT;
        break;
    case INPUT_DIR_RIGHT:
        events |= INPUT_EVENT_RIGHT;
        break;
    default:
        break;
    }

    if (snapshot->joy_sw_pressed)
    {
        events |= INPUT_EVENT_A;
    }
    if (snapshot->key_b_pressed)
    {
        events |= INPUT_EVENT_B;
    }
    if (snapshot->key_start_pressed)
    {
        events |= INPUT_EVENT_START;
    }
    if (snapshot->key_menu_pressed)
    {
        events |= INPUT_EVENT_MENU;
    }
    if (snapshot->key_extra_pressed)
    {
        events |= INPUT_EVENT_EXTRA;
    }

    return events;
}

static void update_repeat_events(uint32_t now)
{
    uint16_t repeatable_events = input_snapshot.held_events & INPUT_DIRECTION_EVENTS_MASK;

    /* 当前只对方向键生成 repeat。确认/返回类按键不重复，避免误触发。 */
    if (repeatable_events == INPUT_EVENT_NONE)
    {
        repeat_reference_events = INPUT_EVENT_NONE;
        input_snapshot.repeat_events = INPUT_EVENT_NONE;
        return;
    }

    if (repeatable_events != repeat_reference_events)
    {
        repeat_reference_events = repeatable_events;
        repeat_start_tick = now;
        repeat_last_tick = now;
        input_snapshot.repeat_events = INPUT_EVENT_NONE;
        return;
    }

    if ((now - repeat_start_tick) < INPUT_REPEAT_START_MS)
    {
        input_snapshot.repeat_events = INPUT_EVENT_NONE;
        return;
    }

    if ((now - repeat_last_tick) >= INPUT_REPEAT_INTERVAL_MS)
    {
        repeat_last_tick = now;
        input_snapshot.repeat_events |= repeatable_events;
    }
}

void Input_Service_Init(void)
{
    input_snapshot.joy_x = 0;
    input_snapshot.joy_y = 0;
    input_snapshot.joy_sw_pressed = 0;
    input_snapshot.key_b_pressed = 0;
    input_snapshot.key_start_pressed = 0;
    input_snapshot.key_menu_pressed = 0;
    input_snapshot.key_extra_pressed = 0;
    input_snapshot.direction = INPUT_DIR_CENTER;
    input_snapshot.held_events = INPUT_EVENT_NONE;
    input_snapshot.pressed_events = INPUT_EVENT_NONE;
    input_snapshot.released_events = INPUT_EVENT_NONE;
    input_snapshot.repeat_events = INPUT_EVENT_NONE;

    previous_held_events = INPUT_EVENT_NONE;
    repeat_reference_events = INPUT_EVENT_NONE;
    repeat_start_tick = 0;
    repeat_last_tick = 0;
    debounce_init(&joy_sw_debounce);
    debounce_init(&key_b_debounce);
    debounce_init(&key_start_debounce);
    debounce_init(&key_menu_debounce);
    debounce_init(&key_extra_debounce);
    event_queue_head = 0U;
    event_queue_tail = 0U;
    event_queue_count = 0U;
}

void Input_Service_Update(void)
{
    uint32_t now = HAL_GetTick();
    uint16_t pressed_delta;
    uint16_t released_delta;

    /* 服务层每次扫描都先读取原始驱动值，再统一生成事件位。 */
    input_snapshot.joy_x = Joystick_ReadX();
    input_snapshot.joy_y = Joystick_ReadY();
    input_snapshot.joy_sw_pressed = debounce_update(&joy_sw_debounce, Joystick_SW_IsPressed());
    input_snapshot.key_b_pressed = debounce_update(&key_b_debounce, Key_B_IsPressed());
    input_snapshot.key_start_pressed = debounce_update(&key_start_debounce, Key_Start_IsPressed());
    input_snapshot.key_menu_pressed = debounce_update(&key_menu_debounce, Key_Menu_IsPressed());
    input_snapshot.key_extra_pressed = debounce_update(&key_extra_debounce, Key_Extra_IsPressed());
    input_snapshot.direction = joystick_direction_from_adc(input_snapshot.joy_x, input_snapshot.joy_y);

    input_snapshot.held_events = events_from_snapshot(&input_snapshot);

    /* 边沿事件为当前扫描瞬时事件，同时写入轻量队列供应用层批量消费。 */
    pressed_delta = input_snapshot.held_events & (uint16_t)(~previous_held_events);
    released_delta = previous_held_events & (uint16_t)(~input_snapshot.held_events);
    input_snapshot.pressed_events = pressed_delta;
    input_snapshot.released_events = released_delta;
    previous_held_events = input_snapshot.held_events;

    update_repeat_events(now);
    enqueue_event_frame(pressed_delta, released_delta, input_snapshot.repeat_events);
}

const InputSnapshot *Input_Service_GetSnapshot(void)
{
    return &input_snapshot;
}

void Input_Service_ClearEdgeEvents(void)
{
    input_snapshot.pressed_events = INPUT_EVENT_NONE;
    input_snapshot.released_events = INPUT_EVENT_NONE;
    input_snapshot.repeat_events = INPUT_EVENT_NONE;
}

uint8_t Input_Service_DequeueEventFrame(InputEventFrame *out_frame)
{
    if ((out_frame == (InputEventFrame *)0) || (event_queue_count == 0U))
    {
        return 0U;
    }

    *out_frame = event_queue[event_queue_head];
    event_queue_head = (uint8_t)((event_queue_head + 1U) % INPUT_EVENT_QUEUE_SIZE);
    event_queue_count--;
    return 1U;
}

const char *Input_DirectionToString(InputDirection direction)
{
    switch (direction)
    {
    case INPUT_DIR_UP:
        return "UP";
    case INPUT_DIR_DOWN:
        return "DOWN";
    case INPUT_DIR_LEFT:
        return "LEFT";
    case INPUT_DIR_RIGHT:
        return "RIGHT";
    default:
        return "CENTER";
    }
}
