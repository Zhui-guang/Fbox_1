#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#define USER_LED_GPIO_PORT GPIOB
#define USER_LED_PIN       GPIO_PIN_2

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
