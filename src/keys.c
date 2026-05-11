#include "keys.h"

static uint8_t key_is_pressed(GPIO_TypeDef *gpio_port, uint16_t gpio_pin)
{
    /* 所有数字按键均采用内部上拉、按下接 GND，因此低电平表示按下。 */
    return HAL_GPIO_ReadPin(gpio_port, gpio_pin) == GPIO_PIN_RESET;
}

void Keys_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 摇杆按下开关：PC13，映射为 A。 */
    GPIO_InitStruct.Pin = JOYSTICK_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(JOYSTICK_SW_GPIO_PORT, &GPIO_InitStruct);

    /* 四个独立按键集中在 PB12~PB15，便于后续统一扫描。 */
    GPIO_InitStruct.Pin = KEY_B_PIN | KEY_START_PIN | KEY_MENU_PIN | KEY_EXTRA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

uint8_t Joystick_SW_ReadRaw(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(JOYSTICK_SW_GPIO_PORT, JOYSTICK_SW_PIN);
}

uint8_t Joystick_SW_IsPressed(void)
{
    return Joystick_SW_ReadRaw() == GPIO_PIN_RESET;
}

uint8_t Key_B_IsPressed(void)
{
    return key_is_pressed(KEY_B_GPIO_PORT, KEY_B_PIN);
}

uint8_t Key_Start_IsPressed(void)
{
    return key_is_pressed(KEY_START_GPIO_PORT, KEY_START_PIN);
}

uint8_t Key_Menu_IsPressed(void)
{
    return key_is_pressed(KEY_MENU_GPIO_PORT, KEY_MENU_PIN);
}

uint8_t Key_Extra_IsPressed(void)
{
    return key_is_pressed(KEY_EXTRA_GPIO_PORT, KEY_EXTRA_PIN);
}
