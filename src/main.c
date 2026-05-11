#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "adc.h"
#include "keys.h"
#include "input_service.h"
#include "audio_service.h"
#include "storage_service.h"
#include "app.h"
#include "spi_lcd.h"
#include "st7789.h"
#include "gfx.h"
#include "ui_layout.h"
#include "ui_widgets.h"
#include "app_error.h"
#include "debug_log.h"
#include "w25q128.h"
#include "pcm_uploader.h"

void SystemClock_Config(void);
static void Error_LED_Init(void);
static void App_FaultLoop(AppErrorCode code, uint8_t lcd_ready);

int main(void)
{
    HAL_StatusTypeDef spi_rc;
    HAL_StatusTypeDef lcd_rc;
    HAL_StatusTypeDef flash_rc;
    uint8_t jedec_mid = 0U;
    uint8_t jedec_type = 0U;
    uint8_t jedec_cap = 0U;
    uint8_t lcd_ready = 0U;

    /* HAL_Init 会初始化 Flash 接口并配置 SysTick。SysTick_Handler 中必须调用
     * HAL_IncTick()，否则 HAL_Delay 和所有基于 HAL_GetTick 的调度都会失效。 */
    HAL_Init();

    /* 使用 CubeMX 示例工程验证过的 8MHz HSE + PLL 168MHz 时钟树。 */
    SystemClock_Config();

    /* 初始化顺序保持“底层外设 -> 服务层 -> 应用层”。后续新增 LCD/SPI/I2S 时，
     * 也应先完成驱动初始化，再启动 App 状态机。 */
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    spi_rc = MX_SPI1_LCD_Init();
    if (spi_rc != HAL_OK)
    {
        App_FaultLoop(APP_ERR_SPI_LCD_INIT, lcd_ready);
    }
    flash_rc = W25Q128_Init();
    if (flash_rc == HAL_OK)
    {
        flash_rc = W25Q128_ReadJedecId(&jedec_mid, &jedec_type, &jedec_cap);
    }
    if (flash_rc == HAL_OK)
    {
        Debug_Log("[FLASH] JEDEC ID: %02X %02X %02X\r\n", jedec_mid, jedec_type, jedec_cap);
    }
    else
    {
        Debug_Log("[FLASH] JEDEC read failed\r\n");
    }
    Keys_Init();
    Input_Service_Init();
    Storage_Service_Init();
    Audio_Service_Init();
    lcd_rc = ST7789_Init();
    if (lcd_rc != HAL_OK)
    {
        App_FaultLoop(APP_ERR_ST7789_INIT, lcd_ready);
    }
    lcd_ready = 1U;
    GFX_Init();

    if (Audio_Service_IsReady() == 0U)
    {
        /* I2S 初始化失败不直接停机，显示故障页后可继续进入 UI。 */
        UIW_DrawDialog("故障", "音频I2S初始化失败", "将以静音模式继续运行", "A/B: 继续");
        Debug_Log("[FAULT] %s\r\n", App_ErrorToString(APP_ERR_AUDIO_I2S_INIT));
        HAL_Delay(1400);
    }

    App_Init();

    while (1)
    {
        PCM_Uploader_Poll();
        /* 裸机主循环只调度应用层；不要在 main 中堆业务逻辑。 */
        App_Update();
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* PLL: 8MHz / 8 * 336 / 2 = 168MHz；PLLQ=7 得到 48MHz 域。 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    /* 168MHz 下 Flash latency 必须为 5，否则运行不稳定。 */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }

    /* CSS 可在外部晶振异常时触发时钟安全机制。 */
    HAL_RCC_EnableCSS();
}

void Error_Handler(void)
{
    __disable_irq();
    Error_LED_Init();

    while (1)
    {
        /* 错误态使用 PB2 快速闪烁，便于区分正常心跳。这里不用 HAL_Delay，
         * 因为错误可能发生在 SysTick 或时钟初始化之前。 */
        HAL_GPIO_TogglePin(USER_LED_GPIO_PORT, USER_LED_PIN);
        for (volatile uint32_t i = 0; i < 800000U; ++i)
        {
        }
    }
}

static void Error_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = USER_LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USER_LED_GPIO_PORT, &GPIO_InitStruct);
}

static void App_FaultLoop(AppErrorCode code, uint8_t lcd_ready)
{
    __disable_irq();
    Error_LED_Init();

    if (lcd_ready != 0U)
    {
        UIW_DrawDialog("FAULT", "System bring-up failed", App_ErrorToString(code), "Power cycle / reflash");
    }

    Debug_Log("[FATAL] %s\r\n", App_ErrorToString(code));
    while (1)
    {
        HAL_GPIO_TogglePin(USER_LED_GPIO_PORT, USER_LED_PIN);
        for (volatile uint32_t i = 0; i < 800000U; ++i)
        {
        }
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
