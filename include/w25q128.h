#ifndef __W25Q128_H__
#define __W25Q128_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

HAL_StatusTypeDef W25Q128_Init(void);
HAL_StatusTypeDef W25Q128_ReadJedecId(uint8_t *mid, uint8_t *type, uint8_t *capacity);
HAL_StatusTypeDef W25Q128_Read(uint32_t addr, uint8_t *buf, uint16_t len);
HAL_StatusTypeDef W25Q128_EraseRange(uint32_t addr, uint32_t len);
HAL_StatusTypeDef W25Q128_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q128_H__ */
