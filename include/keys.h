#ifndef __KEYS_H__
#define __KEYS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define JOYSTICK_SW_GPIO_PORT GPIOC
#define JOYSTICK_SW_PIN       GPIO_PIN_13

#define KEY_B_GPIO_PORT       GPIOB
#define KEY_B_PIN             GPIO_PIN_12
#define KEY_START_GPIO_PORT   GPIOB
#define KEY_START_PIN         GPIO_PIN_13
#define KEY_MENU_GPIO_PORT    GPIOB
#define KEY_MENU_PIN          GPIO_PIN_14
#define KEY_EXTRA_GPIO_PORT   GPIOB
#define KEY_EXTRA_PIN         GPIO_PIN_15

void Keys_Init(void);
uint8_t Joystick_SW_IsPressed(void);
uint8_t Joystick_SW_ReadRaw(void);
uint8_t Key_B_IsPressed(void);
uint8_t Key_Start_IsPressed(void);
uint8_t Key_Menu_IsPressed(void);
uint8_t Key_Extra_IsPressed(void);

#ifdef __cplusplus
}
#endif

#endif /* __KEYS_H__ */
