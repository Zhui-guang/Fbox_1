#include "st7789.h"
#include "spi_lcd.h"

/* Control pins reused from the reference demo to simplify wiring migration. */
#define ST7789_CS_GPIO_PORT   GPIOD
#define ST7789_CS_PIN         GPIO_PIN_1
#define ST7789_RST_GPIO_PORT  GPIOD
#define ST7789_RST_PIN        GPIO_PIN_4
#define ST7789_DC_GPIO_PORT   GPIOD
#define ST7789_DC_PIN         GPIO_PIN_15
#define ST7789_BL_GPIO_PORT   GPIOE
#define ST7789_BL_PIN         GPIO_PIN_8

#define ST7789_X_OFFSET 0U
#define ST7789_Y_OFFSET 20U

#define ST7789_SWRESET 0x01U
#define ST7789_SLPOUT  0x11U
#define ST7789_COLMOD  0x3AU
#define ST7789_MADCTL  0x36U
#define ST7789_CASET   0x2AU
#define ST7789_RASET   0x2BU
#define ST7789_RAMWR   0x2CU
#define ST7789_INVON   0x21U
#define ST7789_DISPON  0x29U

static ST7789_Rotation g_rotation = ST7789_ROTATION_0;

static void st7789_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin = ST7789_CS_PIN | ST7789_RST_PIN | ST7789_DC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ST7789_BL_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    HAL_GPIO_WritePin(ST7789_CS_GPIO_PORT, ST7789_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_DC_GPIO_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_RST_GPIO_PORT, ST7789_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_BL_GPIO_PORT, ST7789_BL_PIN, GPIO_PIN_SET);
}

static void st7789_select(void)
{
    HAL_GPIO_WritePin(ST7789_CS_GPIO_PORT, ST7789_CS_PIN, GPIO_PIN_RESET);
}

static void st7789_unselect(void)
{
    HAL_GPIO_WritePin(ST7789_CS_GPIO_PORT, ST7789_CS_PIN, GPIO_PIN_SET);
}

static void st7789_write_cmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(ST7789_DC_GPIO_PORT, ST7789_DC_PIN, GPIO_PIN_RESET);
    st7789_select();
    HAL_SPI_Transmit(&hspi1, &cmd, 1U, HAL_MAX_DELAY);
    st7789_unselect();
}

static void st7789_write_data(const uint8_t *data, uint16_t size)
{
    HAL_GPIO_WritePin(ST7789_DC_GPIO_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    st7789_select();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, HAL_MAX_DELAY);
    st7789_unselect();
}

static void st7789_write_u8(uint8_t data)
{
    st7789_write_data(&data, 1U);
}

static void st7789_hw_reset(void)
{
    HAL_GPIO_WritePin(ST7789_RST_GPIO_PORT, ST7789_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(ST7789_RST_GPIO_PORT, ST7789_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);
}

static void st7789_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    x0 += ST7789_X_OFFSET;
    x1 += ST7789_X_OFFSET;
    y0 += ST7789_Y_OFFSET;
    y1 += ST7789_Y_OFFSET;

    st7789_write_cmd(ST7789_CASET);
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)(x0 & 0xFF);
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)(x1 & 0xFF);
    st7789_write_data(data, 4U);

    st7789_write_cmd(ST7789_RASET);
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)(y0 & 0xFF);
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)(y1 & 0xFF);
    st7789_write_data(data, 4U);

    st7789_write_cmd(ST7789_RAMWR);
}

void ST7789_SetRotation(ST7789_Rotation rotation)
{
    uint8_t madctl = 0x00U;

    g_rotation = rotation;

    switch (rotation)
    {
    case ST7789_ROTATION_0:
        madctl = 0x00U;
        break;
    case ST7789_ROTATION_90:
        madctl = 0x60U;
        break;
    case ST7789_ROTATION_180:
        madctl = 0xC0U;
        break;
    case ST7789_ROTATION_270:
        madctl = 0xA0U;
        break;
    default:
        madctl = 0x00U;
        break;
    }

    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_u8(madctl);
}

void ST7789_Init(void)
{
    static const uint8_t b2_data[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
    static const uint8_t d0_data[] = {0xA4U, 0xA1U};
    static const uint8_t e0_data[] = {0xD0U, 0x08U, 0x0EU, 0x09U, 0x09U, 0x05U, 0x31U, 0x33U, 0x48U, 0x17U, 0x14U, 0x15U, 0x31U, 0x34U};
    static const uint8_t e1_data[] = {0xD0U, 0x08U, 0x0EU, 0x09U, 0x09U, 0x15U, 0x31U, 0x33U, 0x48U, 0x17U, 0x14U, 0x15U, 0x31U, 0x34U};

    st7789_gpio_init();
    st7789_hw_reset();

    st7789_write_cmd(ST7789_SWRESET);
    HAL_Delay(150);
    st7789_write_cmd(ST7789_SLPOUT);
    HAL_Delay(120);

    ST7789_SetRotation(ST7789_ROTATION_0);

    st7789_write_cmd(ST7789_COLMOD);
    st7789_write_u8(0x05U);

    st7789_write_cmd(0xB2U);
    st7789_write_data(b2_data, sizeof(b2_data));

    st7789_write_cmd(0xB7U);
    st7789_write_u8(0x35U);

    st7789_write_cmd(0xBBU);
    st7789_write_u8(0x32U);

    st7789_write_cmd(0xC2U);
    st7789_write_u8(0x01U);

    st7789_write_cmd(0xC3U);
    st7789_write_u8(0x15U);

    st7789_write_cmd(0xC4U);
    st7789_write_u8(0x20U);

    st7789_write_cmd(0xC6U);
    st7789_write_u8(0x0FU);

    st7789_write_cmd(0xD0U);
    st7789_write_data(d0_data, sizeof(d0_data));

    st7789_write_cmd(0xE0U);
    st7789_write_data(e0_data, sizeof(e0_data));

    st7789_write_cmd(0xE1U);
    st7789_write_data(e1_data, sizeof(e1_data));

    st7789_write_cmd(ST7789_INVON);
    st7789_write_cmd(ST7789_DISPON);
    HAL_Delay(50);
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t data[2];

    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT))
    {
        return;
    }

    st7789_set_address_window(x, y, x, y);
    data[0] = (uint8_t)(color >> 8);
    data[1] = (uint8_t)(color & 0xFF);
    st7789_write_data(data, 2U);
}

void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t data[2];
    uint32_t total;
    uint32_t i;

    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT) || (w == 0U) || (h == 0U))
    {
        return;
    }

    if ((x + w) > ST7789_WIDTH)
    {
        w = ST7789_WIDTH - x;
    }
    if ((y + h) > ST7789_HEIGHT)
    {
        h = ST7789_HEIGHT - y;
    }

    st7789_set_address_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    data[0] = (uint8_t)(color >> 8);
    data[1] = (uint8_t)(color & 0xFF);

    HAL_GPIO_WritePin(ST7789_DC_GPIO_PORT, ST7789_DC_PIN, GPIO_PIN_SET);
    st7789_select();
    total = (uint32_t)w * (uint32_t)h;
    for (i = 0; i < total; ++i)
    {
        HAL_SPI_Transmit(&hspi1, data, 2U, HAL_MAX_DELAY);
    }
    st7789_unselect();
}

void ST7789_FillScreen(uint16_t color)
{
    ST7789_FillRect(0U, 0U, ST7789_WIDTH, ST7789_HEIGHT, color);
}

void ST7789_TestPattern(void)
{
    uint16_t bar_h = ST7789_HEIGHT / 4U;

    ST7789_FillRect(0U, 0U, ST7789_WIDTH, bar_h, 0xF800U);             /* Red */
    ST7789_FillRect(0U, bar_h, ST7789_WIDTH, bar_h, 0x07E0U);          /* Green */
    ST7789_FillRect(0U, (uint16_t)(bar_h * 2U), ST7789_WIDTH, bar_h, 0x001FU); /* Blue */
    ST7789_FillRect(0U, (uint16_t)(bar_h * 3U), ST7789_WIDTH,
                    (uint16_t)(ST7789_HEIGHT - bar_h * 3U), 0xFFFFU);  /* White */
}

