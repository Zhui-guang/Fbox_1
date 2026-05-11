#ifndef __SCREEN_H__
#define __SCREEN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void Screen_Clear(uint16_t color);
void Screen_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void Screen_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg);

#ifdef __cplusplus
}
#endif

#endif /* __SCREEN_H__ */
