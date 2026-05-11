#include "adc.h"

ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    /* PA0/PA1 分别接摇杆 X/Y。ADC 引脚必须使用模拟模式并关闭上下拉。 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 当前采用单次转换 + 软件触发。后续如需更高采样率，可再改为扫描 + DMA。 */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }
}

uint16_t ADC1_ReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef channel_config = {0};
    uint16_t value = 0;

    /* 每次读取前重新配置通道，简单可靠；当前输入扫描周期低，不需要 DMA。 */
    channel_config.Channel = channel;
    channel_config.Rank = 1;
    channel_config.SamplingTime = ADC_SAMPLETIME_144CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);
    return value;
}

uint16_t Joystick_ReadX(void)
{
    return ADC1_ReadChannel(ADC_CHANNEL_0);
}

uint16_t Joystick_ReadY(void)
{
    return ADC1_ReadChannel(ADC_CHANNEL_1);
}
