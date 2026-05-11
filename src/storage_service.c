#include "storage_service.h"
#include "debug_log.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"

#define STORAGE_MAGIC             0x46425831UL /* "FBX1" */
#define STORAGE_VERSION           0x0004U
#define STORAGE_PAGE_A_ADDR       0x080C0000UL /* Sector 10 */
#define STORAGE_PAGE_B_ADDR       0x080E0000UL /* Sector 11 */
#define STORAGE_PAGE_A_SECTOR     FLASH_SECTOR_10
#define STORAGE_PAGE_B_SECTOR     FLASH_SECTOR_11
#define STORAGE_WRITE_DELAY_MS    600U

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    uint32_t sequence;
    StorageRuntimeConfig cfg;
    uint32_t crc32;
} StorageBlob;

static StorageRuntimeConfig g_cfg;
static uint8_t g_dirty;
static uint32_t g_dirty_tick;
static uint8_t g_active_page; /* 0=A, 1=B */
static uint32_t g_sequence;

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
    g_cfg.snake_difficulty = 2U;
    g_cfg.brick_difficulty = 2U;
    g_cfg.brick_init_lives = 3U;
    g_cfg.reserved0 = 0U;
    g_cfg.snake_high_score = 0U;
    g_cfg.brick_high_score = 0U;
}

static uint8_t storage_validate_blob(const StorageBlob *blob)
{
    uint32_t crc_expected;

    if (blob == (const StorageBlob *)0)
    {
        return 0U;
    }
    if (blob->magic != STORAGE_MAGIC)
    {
        return 0U;
    }
    if (blob->version != STORAGE_VERSION)
    {
        return 0U;
    }
    if (blob->payload_size != sizeof(StorageRuntimeConfig))
    {
        return 0U;
    }

    crc_expected = storage_crc32((const uint8_t *)&blob->cfg, sizeof(StorageRuntimeConfig));
    if (crc_expected != blob->crc32)
    {
        return 0U;
    }
    return 1U;
}

static uint8_t storage_load_latest(StorageBlob *out_blob, uint8_t *out_page)
{
    const StorageBlob *blob_a = (const StorageBlob *)STORAGE_PAGE_A_ADDR;
    const StorageBlob *blob_b = (const StorageBlob *)STORAGE_PAGE_B_ADDR;
    uint8_t valid_a;
    uint8_t valid_b;

    if ((out_blob == (StorageBlob *)0) || (out_page == (uint8_t *)0))
    {
        return 0U;
    }

    valid_a = storage_validate_blob(blob_a);
    valid_b = storage_validate_blob(blob_b);

    if ((valid_a == 0U) && (valid_b == 0U))
    {
        return 0U;
    }

    if ((valid_a != 0U) && (valid_b == 0U))
    {
        *out_blob = *blob_a;
        *out_page = 0U;
        return 1U;
    }

    if ((valid_a == 0U) && (valid_b != 0U))
    {
        *out_blob = *blob_b;
        *out_page = 1U;
        return 1U;
    }

    if (blob_b->sequence >= blob_a->sequence)
    {
        *out_blob = *blob_b;
        *out_page = 1U;
    }
    else
    {
        *out_blob = *blob_a;
        *out_page = 0U;
    }

    return 1U;
}

static uint8_t storage_write_blob_to_page(const StorageBlob *blob, uint8_t page)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    uint32_t addr;
    const uint32_t *words;
    uint32_t word_count;
    uint32_t i;

    if ((blob == (const StorageBlob *)0) || (page > 1U))
    {
        return 0U;
    }

    addr = (page == 0U) ? STORAGE_PAGE_A_ADDR : STORAGE_PAGE_B_ADDR;
    words = (const uint32_t *)blob;
    word_count = (uint32_t)((sizeof(StorageBlob) + 3U) / 4U);

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = (page == 0U) ? STORAGE_PAGE_A_SECTOR : STORAGE_PAGE_B_SECTOR;
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
    g_active_page = 0U;
    g_sequence = 0U;

    if (storage_load_latest(&blob, &g_active_page) != 0U)
    {
        g_cfg = blob.cfg;
        g_sequence = blob.sequence;

        if (g_cfg.volume_percent > 100U)
        {
            g_cfg.volume_percent = 1U;
        }

        Debug_Log("[STORAGE] load ok: page=%c seq=%lu sound=%u vol=%u snake=%u brick=%u\r\n",
                  (g_active_page == 0U) ? 'A' : 'B',
                  (unsigned long)g_sequence,
                  g_cfg.sound_enabled,
                  g_cfg.volume_percent,
                  g_cfg.snake_high_score,
                  g_cfg.brick_high_score);
        if ((g_cfg.snake_difficulty < 1U) || (g_cfg.snake_difficulty > 3U)) g_cfg.snake_difficulty = 2U;
        if ((g_cfg.brick_difficulty < 1U) || (g_cfg.brick_difficulty > 3U)) g_cfg.brick_difficulty = 2U;
        if ((g_cfg.brick_init_lives < 1U) || (g_cfg.brick_init_lives > 5U)) g_cfg.brick_init_lives = 3U;
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
    uint8_t next_page;

    if (g_dirty == 0U)
    {
        return;
    }

    if ((HAL_GetTick() - g_dirty_tick) < STORAGE_WRITE_DELAY_MS)
    {
        return;
    }

    next_page = (uint8_t)(g_active_page ^ 1U);

    blob.magic = STORAGE_MAGIC;
    blob.version = STORAGE_VERSION;
    blob.payload_size = sizeof(StorageRuntimeConfig);
    blob.sequence = g_sequence + 1U;
    blob.cfg = g_cfg;
    blob.crc32 = storage_crc32((const uint8_t *)&blob.cfg, sizeof(StorageRuntimeConfig));

    if (storage_write_blob_to_page(&blob, next_page) != 0U)
    {
        g_dirty = 0U;
        g_active_page = next_page;
        g_sequence = blob.sequence;
        Debug_Log("[STORAGE] save ok: page=%c seq=%lu\r\n",
                  (g_active_page == 0U) ? 'A' : 'B',
                  (unsigned long)g_sequence);
    }
    else
    {
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

uint16_t Storage_Service_GetBrickHighScore(void)
{
    return g_cfg.brick_high_score;
}
uint8_t Storage_Service_GetSnakeDifficulty(void)
{
    return g_cfg.snake_difficulty;
}
uint8_t Storage_Service_GetBrickDifficulty(void)
{
    return g_cfg.brick_difficulty;
}
uint8_t Storage_Service_GetBrickInitLives(void)
{
    return g_cfg.brick_init_lives;
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

void Storage_Service_SetBrickHighScore(uint16_t high_score)
{
    if (g_cfg.brick_high_score != high_score)
    {
        g_cfg.brick_high_score = high_score;
        storage_mark_dirty();
    }
}
void Storage_Service_SetSnakeDifficulty(uint8_t level)
{
    if (level < 1U) level = 1U;
    if (level > 3U) level = 3U;
    if (g_cfg.snake_difficulty != level)
    {
        g_cfg.snake_difficulty = level;
        storage_mark_dirty();
    }
}
void Storage_Service_SetBrickDifficulty(uint8_t level)
{
    if (level < 1U) level = 1U;
    if (level > 3U) level = 3U;
    if (g_cfg.brick_difficulty != level)
    {
        g_cfg.brick_difficulty = level;
        storage_mark_dirty();
    }
}
void Storage_Service_SetBrickInitLives(uint8_t lives)
{
    if (lives < 1U) lives = 1U;
    if (lives > 5U) lives = 5U;
    if (g_cfg.brick_init_lives != lives)
    {
        g_cfg.brick_init_lives = lives;
        storage_mark_dirty();
    }
}
