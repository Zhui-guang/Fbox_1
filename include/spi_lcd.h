#ifndef __SPI_LCD_H__
#define __SPI_LCD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern SPI_HandleTypeDef hspi1;

void MX_SPI1_LCD_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_LCD_H__ */

