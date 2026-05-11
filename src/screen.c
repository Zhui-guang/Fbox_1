#include "screen.h"
#include "gfx.h"

void Screen_Clear(uint16_t color)
{
    GFX_Clear(color);
}

void Screen_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    GFX_FillRect(x, y, w, h, color);
}

void Screen_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg)
{
    GFX_DrawText8x16(x, y, text, fg, bg);
}
