#ifndef __INPUT_SERVICE_H__
#define __INPUT_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 摇杆方向经过 ADC 阈值和回滞处理后，只输出这 5 种离散方向。 */
typedef enum
{
    INPUT_DIR_CENTER = 0,
    INPUT_DIR_UP,
    INPUT_DIR_DOWN,
    INPUT_DIR_LEFT,
    INPUT_DIR_RIGHT
} InputDirection;

/*
 * 统一输入事件位图。
 * App / 游戏层只依赖这些事件，不直接读取 ADC/GPIO，后续换引脚或换输入硬件时影响较小。
 */
typedef enum
{
    INPUT_EVENT_NONE  = 0x0000U,
    INPUT_EVENT_UP    = 0x0001U,
    INPUT_EVENT_DOWN  = 0x0002U,
    INPUT_EVENT_LEFT  = 0x0004U,
    INPUT_EVENT_RIGHT = 0x0008U,
    INPUT_EVENT_A     = 0x0010U,
    INPUT_EVENT_B     = 0x0020U,
    INPUT_EVENT_START = 0x0040U,
    INPUT_EVENT_MENU  = 0x0080U,
    INPUT_EVENT_EXTRA = 0x0100U
} InputEvent;

/*
 * 一帧输入快照。
 * joy_x/joy_y 保留原始 ADC 值用于校准和调试；
 * *_pressed 是当前稳定电平状态；
 * held_events 表示当前持续按下；
 * pressed/released/repeat 是边沿事件，供菜单和游戏逻辑消费。
 */
typedef struct
{
    uint16_t joy_x;
    uint16_t joy_y;
    uint8_t joy_sw_pressed;
    uint8_t key_b_pressed;
    uint8_t key_start_pressed;
    uint8_t key_menu_pressed;
    uint8_t key_extra_pressed;
    InputDirection direction;
    uint16_t held_events;
    uint16_t pressed_events;
    uint16_t released_events;
    uint16_t repeat_events;
} InputSnapshot;

typedef struct
{
    uint16_t pressed_events;
    uint16_t released_events;
    uint16_t repeat_events;
} InputEventFrame;

void Input_Service_Init(void);
void Input_Service_Update(void);
const InputSnapshot *Input_Service_GetSnapshot(void);
void Input_Service_ClearEdgeEvents(void);
uint8_t Input_Service_DequeueEventFrame(InputEventFrame *out_frame);
const char *Input_DirectionToString(InputDirection direction);

#ifdef __cplusplus
}
#endif

#endif /* __INPUT_SERVICE_H__ */
