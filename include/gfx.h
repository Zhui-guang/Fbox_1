#ifndef __GFX_H__
#define __GFX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void GFX_Init(void);
void GFX_Clear(uint16_t color);
void GFX_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void GFX_DrawChar8x16(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg);
void GFX_DrawText8x16(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg);

#ifdef __cplusplus
}
#endif

#endif /* __GFX_H__ */

