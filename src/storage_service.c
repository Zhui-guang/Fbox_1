#include "storage_service.h"
#include "debug_log.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"

#define STORAGE_MAGIC            0x46425831UL /* "FBX1" */
#define STORAGE_VERSION          0x0001U
#define STORAGE_FLASH_ADDR       0x080E0000UL /* STM32F407 Sector 11 */
#define STORAGE_FLASH_SECTOR     FLASH_SECTOR_11
#define STORAGE_WRITE_DELAY_MS   600U

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    StorageRuntimeConfig cfg;
    uint32_t crc32;
} StorageBlob;

static StorageRuntimeConfig g_cfg;
static uint8_t g_dirty;
static uint32_t g_dirty_tick;

static uint32_t storage_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    uint8_t b;

    while (len-- > 0U)
    {
        b = *data++;
        crc ^= b;
        for (i = 0U; i < 8U; ++i)
        {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1UL));
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

static void storage_set_defaults(void)
{
    g_cfg.sound_enabled = 0U;
    g_cfg.volume_percent = 1U;
    g_cfg.reserved = 0U;
    g_cfg.snake_high_score = 0U;
}

static uint8_t storage_load_blob(StorageBlob *out_blob)
{
    const StorageBlob *flash_blob = (const StorageBlob *)STORAGE_FLASH_ADDR;
    uint32_t crc_expected;

    if (out_blob == (StorageBlob *)0)
    {
        return 0U;
    }

    *out_blob = *flash_blob;
    if (out_blob->magic != STORAGE_MAGIC)
    {
        return 0U;
    }
    if (out_blob->version != STORAGE_VERSION)
    {
        return 0U;
    }
    if (out_blob->payload_size != sizeof(StorageRuntimeConfig))
    {
        return 0U;
    }

    crc_expected = storage_crc32((const uint8_t *)&out_blob->cfg, sizeof(StorageRuntimeConfig));
    if (crc_expected != out_blob->crc32)
    {
        return 0U;
    }
    return 1U;
}

static uint8_t storage_write_blob(const StorageBlob *blob)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    uint32_t addr = STORAGE_FLASH_ADDR;
    const uint32_t *words = (const uint32_t *)blob;
    uint32_t word_count = (uint32_t)((sizeof(StorageBlob) + 3U) / 4U);
    uint32_t i;

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = STORAGE_FLASH_SECTOR;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0U;
    }

    for (i = 0U; i < word_count; ++i)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, words[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0U;
        }
        addr += 4U;
    }

    HAL_FLASH_Lock();
    return 1U;
}

static void storage_mark_dirty(void)
{
    g_dirty = 1U;
    g_dirty_tick = HAL_GetTick();
}

void Storage_Service_Init(void)
{
    StorageBlob blob;

    storage_set_defaults();
    if (storage_load_blob(&blob) != 0U)
    {
        g_cfg = blob.cfg;
        if (g_cfg.volume_percent > 100U)
        {
            g_cfg.volume_percent = 1U;
        }
        Debug_Log("[STORAGE] load ok: sound=%u vol=%u high=%u\r\n",
                  g_cfg.sound_enabled, g_cfg.volume_percent, g_cfg.snake_high_score);
    }
    else
    {
        Debug_Log("[STORAGE] load miss, defaults applied\r\n");
        storage_mark_dirty();
    }
}

void Storage_Service_Process(void)
{
    StorageBlob blob;

    if (g_dirty == 0U)
    {
        return;
    }

    if ((HAL_GetTick() - g_dirty_tick) < STORAGE_WRITE_DELAY_MS)
    {
        return;
    }

    blob.magic = STORAGE_MAGIC;
    blob.version = STORAGE_VERSION;
    blob.payload_size = sizeof(StorageRuntimeConfig);
    blob.cfg = g_cfg;
    blob.crc32 = storage_crc32((const uint8_t *)&blob.cfg, sizeof(StorageRuntimeConfig));

    if (storage_write_blob(&blob) != 0U)
    {
        g_dirty = 0U;
        Debug_Log("[STORAGE] save ok\r\n");
    }
    else
    {
        /* 保持 dirty，后续继续尝试。 */
        g_dirty_tick = HAL_GetTick();
        Debug_Log("[STORAGE] save fail, retry\r\n");
    }
}

uint8_t Storage_Service_GetSoundEnabled(void)
{
    return g_cfg.sound_enabled;
}

uint8_t Storage_Service_GetVolumePercent(void)
{
    return g_cfg.volume_percent;
}

uint16_t Storage_Service_GetSnakeHighScore(void)
{
    return g_cfg.snake_high_score;
}

void Storage_Service_SetSoundEnabled(uint8_t enabled)
{
    uint8_t v = (enabled != 0U) ? 1U : 0U;
    if (g_cfg.sound_enabled != v)
    {
        g_cfg.sound_enabled = v;
        storage_mark_dirty();
    }
}

void Storage_Service_SetVolumePercent(uint8_t volume_percent)
{
    if (volume_percent > 100U)
    {
        volume_percent = 100U;
    }
    if (g_cfg.volume_percent != volume_percent)
    {
        g_cfg.volume_percent = volume_percent;
        storage_mark_dirty();
    }
}

void Storage_Service_SetSnakeHighScore(uint16_t high_score)
{
    if (g_cfg.snake_high_score != high_score)
    {
        g_cfg.snake_high_score = high_score;
        storage_mark_dirty();
    }
}
