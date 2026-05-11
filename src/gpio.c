#include "gpio.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* CubeMX baseline enables HSE pins on GPIOH and SWD pins stay on PA13/PA14. */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Board-level validation output: user confirmed the only user LED is PB2. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(USER_LED_GPIO_PORT, USER_LED_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = USER_LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USER_LED_GPIO_PORT, &GPIO_InitStruct);
}
