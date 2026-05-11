#ifndef __ST7789_H__
#define __ST7789_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define ST7789_WIDTH  240U
#define ST7789_HEIGHT 280U

typedef enum
{
    ST7789_ROTATION_0 = 0,
    ST7789_ROTATION_90,
    ST7789_ROTATION_180,
    ST7789_ROTATION_270
} ST7789_Rotation;

HAL_StatusTypeDef ST7789_Init(void);
void ST7789_SetRotation(ST7789_Rotation rotation);
void ST7789_FillScreen(uint16_t color);
void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_DrawRGB565Bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);
void ST7789_TestPattern(void);

#ifdef __cplusplus
}
#endif

#endif /* __ST7789_H__ */
