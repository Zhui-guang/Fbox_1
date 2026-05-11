#include "pcm_uploader.h"
#include "audio_service.h"
#include "debug_log.h"
#include "usart.h"
#include "w25q128.h"

#define PCM_UP_CMD            "PCMUPLOAD\n"
#define PCM_UP_CHUNK_MAX      2048U

static uint8_t cmd_match_idx;

static HAL_StatusTypeDef uart_recv_exact(uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    if ((buf == (uint8_t *)0) || (len == 0U))
    {
        return HAL_ERROR;
    }
    return HAL_UART_Receive(&huart2, buf, len, timeout_ms);
}

static HAL_StatusTypeDef uart_send(const uint8_t *buf, uint16_t len)
{
    if ((buf == (const uint8_t *)0) || (len == 0U))
    {
        return HAL_ERROR;
    }
    return HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

static uint32_t le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void pcm_uploader_run(void)
{
    uint8_t hdr[8];
    uint8_t len_buf[2];
    uint8_t chunk[PCM_UP_CHUNK_MAX];
    uint8_t ack = 0x06U;
    uint32_t addr;
    uint32_t total;
    uint32_t written = 0U;
    HAL_StatusTypeDef rc;
    const uint8_t ready[] = "READY\n";
    const uint8_t hdr_ok[] = "HDR\n";
    const uint8_t ers_ok[] = "ERS\n";
    const uint8_t done[] = "DONE\n";
    const uint8_t fail[] = "FAIL\n";

    (void)uart_send(ready, (uint16_t)sizeof(ready) - 1U);

    rc = uart_recv_exact(hdr, sizeof(hdr), 10000U);
    if (rc != HAL_OK)
    {
        (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
        return;
    }
    addr = le32(&hdr[0]);
    total = le32(&hdr[4]);
    if ((total == 0U) || (total > (15U * 1024U * 1024U)))
    {
        (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
        return;
    }

    (void)uart_send(hdr_ok, (uint16_t)sizeof(hdr_ok) - 1U);
    Debug_Log("[UP] erase addr=0x%08lX len=%lu\r\n", (unsigned long)addr, (unsigned long)total);
    rc = W25Q128_EraseRange(addr, total);
    if (rc != HAL_OK)
    {
        (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
        return;
    }
    (void)uart_send(ers_ok, (uint16_t)sizeof(ers_ok) - 1U);

    while (written < total)
    {
        uint16_t chunk_len;
        uint16_t page_off;
        uint16_t copy;
        uint32_t remain;

        rc = uart_recv_exact(len_buf, 2U, 10000U);
        if (rc != HAL_OK)
        {
            (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
            return;
        }
        chunk_len = (uint16_t)(len_buf[0] | ((uint16_t)len_buf[1] << 8));
        if ((chunk_len == 0U) || (chunk_len > PCM_UP_CHUNK_MAX))
        {
            (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
            return;
        }
        remain = total - written;
        if (chunk_len > remain)
        {
            chunk_len = (uint16_t)remain;
        }

        rc = uart_recv_exact(chunk, chunk_len, 10000U);
        if (rc != HAL_OK)
        {
            (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
            return;
        }

        copy = 0U;
        while (copy < chunk_len)
        {
            uint32_t cur_addr = addr + written;
            page_off = (uint16_t)(cur_addr & 0xFFU);
            remain = (uint32_t)chunk_len - copy;
            if (remain > (256U - page_off))
            {
                remain = 256U - page_off;
            }

            rc = W25Q128_PageProgram(cur_addr, &chunk[copy], (uint16_t)remain);
            if (rc != HAL_OK)
            {
                (void)uart_send(fail, (uint16_t)sizeof(fail) - 1U);
                return;
            }
            copy += (uint16_t)remain;
            written += remain;
        }

        (void)uart_send(&ack, 1U);
    }

    (void)uart_send(done, (uint16_t)sizeof(done) - 1U);
    Debug_Log("[UP] done bytes=%lu\r\n", (unsigned long)written);
}

void PCM_Uploader_Poll(void)
{
    uint8_t ch;
    HAL_StatusTypeDef rc;

    rc = HAL_UART_Receive(&huart2, &ch, 1U, 0U);
    if (rc != HAL_OK)
    {
        return;
    }

    if (ch == (uint8_t)PCM_UP_CMD[cmd_match_idx])
    {
        cmd_match_idx++;
        if (cmd_match_idx >= (sizeof(PCM_UP_CMD) - 1U))
        {
            cmd_match_idx = 0U;
            Audio_Service_StopMusic();
            Audio_Service_StopPcmStream();
            pcm_uploader_run();
        }
    }
    else
    {
        cmd_match_idx = 0U;
    }
}
