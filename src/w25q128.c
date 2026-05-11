#include "w25q128.h"
#include "spi_lcd.h"

#define W25Q128_CS_GPIO_PORT    GPIOA
#define W25Q128_CS_PIN          GPIO_PIN_4

#define W25Q128_CMD_JEDEC_ID    0x9FU
#define W25Q128_CMD_READ_DATA   0x03U
#define W25Q128_CMD_WRITE_EN    0x06U
#define W25Q128_CMD_READ_SR1    0x05U
#define W25Q128_CMD_PAGE_PROG   0x02U
#define W25Q128_CMD_SECTOR_4K   0x20U

#define W25Q128_PAGE_SIZE       256U
#define W25Q128_SECTOR_SIZE     4096U

static void w25q128_cs_low(void)
{
    HAL_GPIO_WritePin(W25Q128_CS_GPIO_PORT, W25Q128_CS_PIN, GPIO_PIN_RESET);
}

static void w25q128_cs_high(void)
{
    HAL_GPIO_WritePin(W25Q128_CS_GPIO_PORT, W25Q128_CS_PIN, GPIO_PIN_SET);
}

HAL_StatusTypeDef W25Q128_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = W25Q128_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(W25Q128_CS_GPIO_PORT, &GPIO_InitStruct);
    w25q128_cs_high();

    return HAL_OK;
}

HAL_StatusTypeDef W25Q128_ReadJedecId(uint8_t *mid, uint8_t *type, uint8_t *capacity)
{
    HAL_StatusTypeDef rc;
    uint8_t cmd = W25Q128_CMD_JEDEC_ID;
    uint8_t id[3] = {0};

    if ((mid == (uint8_t *)0) || (type == (uint8_t *)0) || (capacity == (uint8_t *)0))
    {
        return HAL_ERROR;
    }

    w25q128_cs_low();
    rc = HAL_SPI_Transmit(&hspi1, &cmd, 1U, HAL_MAX_DELAY);
    if (rc == HAL_OK)
    {
        rc = HAL_SPI_Receive(&hspi1, id, 3U, HAL_MAX_DELAY);
    }
    w25q128_cs_high();

    if (rc != HAL_OK)
    {
        return rc;
    }

    *mid = id[0];
    *type = id[1];
    *capacity = id[2];
    return HAL_OK;
}

HAL_StatusTypeDef W25Q128_Read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef rc;
    uint8_t hdr[4];

    if ((buf == (uint8_t *)0) || (len == 0U))
    {
        return HAL_ERROR;
    }

    hdr[0] = W25Q128_CMD_READ_DATA;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    w25q128_cs_low();
    rc = HAL_SPI_Transmit(&hspi1, hdr, 4U, HAL_MAX_DELAY);
    if (rc == HAL_OK)
    {
        rc = HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);
    }
    w25q128_cs_high();

    return rc;
}

static HAL_StatusTypeDef w25q128_write_enable(void)
{
    HAL_StatusTypeDef rc;
    uint8_t cmd = W25Q128_CMD_WRITE_EN;
    w25q128_cs_low();
    rc = HAL_SPI_Transmit(&hspi1, &cmd, 1U, HAL_MAX_DELAY);
    w25q128_cs_high();
    return rc;
}

static HAL_StatusTypeDef w25q128_read_sr1(uint8_t *sr1)
{
    HAL_StatusTypeDef rc;
    uint8_t cmd = W25Q128_CMD_READ_SR1;
    if (sr1 == (uint8_t *)0)
    {
        return HAL_ERROR;
    }
    w25q128_cs_low();
    rc = HAL_SPI_Transmit(&hspi1, &cmd, 1U, HAL_MAX_DELAY);
    if (rc == HAL_OK)
    {
        rc = HAL_SPI_Receive(&hspi1, sr1, 1U, HAL_MAX_DELAY);
    }
    w25q128_cs_high();
    return rc;
}

static HAL_StatusTypeDef w25q128_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t sr1 = 0U;
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (w25q128_read_sr1(&sr1) != HAL_OK)
        {
            return HAL_ERROR;
        }
        if ((sr1 & 0x01U) == 0U)
        {
            return HAL_OK;
        }
    }
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef W25Q128_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef rc;
    uint8_t hdr[4];
    uint32_t page_off = addr % W25Q128_PAGE_SIZE;

    if ((buf == (const uint8_t *)0) || (len == 0U))
    {
        return HAL_ERROR;
    }
    if ((page_off + len) > W25Q128_PAGE_SIZE)
    {
        return HAL_ERROR;
    }
    rc = w25q128_write_enable();
    if (rc != HAL_OK)
    {
        return rc;
    }

    hdr[0] = W25Q128_CMD_PAGE_PROG;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    w25q128_cs_low();
    rc = HAL_SPI_Transmit(&hspi1, hdr, 4U, HAL_MAX_DELAY);
    if (rc == HAL_OK)
    {
        rc = HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, HAL_MAX_DELAY);
    }
    w25q128_cs_high();
    if (rc != HAL_OK)
    {
        return rc;
    }
    return w25q128_wait_ready(100U);
}

static HAL_StatusTypeDef w25q128_sector_erase(uint32_t addr)
{
    HAL_StatusTypeDef rc;
    uint8_t hdr[4];

    rc = w25q128_write_enable();
    if (rc != HAL_OK)
    {
        return rc;
    }
    hdr[0] = W25Q128_CMD_SECTOR_4K;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    w25q128_cs_low();
    rc = HAL_SPI_Transmit(&hspi1, hdr, 4U, HAL_MAX_DELAY);
    w25q128_cs_high();
    if (rc != HAL_OK)
    {
        return rc;
    }
    return w25q128_wait_ready(500U);
}

HAL_StatusTypeDef W25Q128_EraseRange(uint32_t addr, uint32_t len)
{
    uint32_t start;
    uint32_t end;
    uint32_t p;
    HAL_StatusTypeDef rc;

    if (len == 0U)
    {
        return HAL_OK;
    }
    start = addr & ~(W25Q128_SECTOR_SIZE - 1U);
    end = (addr + len - 1U) & ~(W25Q128_SECTOR_SIZE - 1U);
    for (p = start; p <= end; p += W25Q128_SECTOR_SIZE)
    {
        rc = w25q128_sector_erase(p);
        if (rc != HAL_OK)
        {
            return rc;
        }
    }
    return HAL_OK;
}
