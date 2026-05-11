#include "gfx.h"
#include "font_ascii_1608.h"
#include "st7789.h"

void GFX_Init(void)
{
    /* Reserved for future stateful caches/dirty-rect context. */
}

void GFX_Clear(uint16_t color)
{
    ST7789_FillScreen(color);
}

void GFX_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ST7789_FillRect(x, y, w, h, color);
}

void GFX_DrawChar8x16(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg)
{
    uint8_t row;
    uint8_t col;
    const uint8_t *glyph;

    if ((uint8_t)ch < 32U || (uint8_t)ch > 126U)
    {
        ch = '?';
    }

    glyph = ascii_1608[(uint8_t)ch - 32U];

    for (row = 0U; row < 16U; ++row)
    {
        uint8_t bits = glyph[row];
        for (col = 0U; col < 8U; ++col)
        {
            if ((bits & (1U << col)) != 0U)
            {
                ST7789_DrawPixel((uint16_t)(x + col), (uint16_t)(y + row), fg);
            }
            else
            {
                ST7789_DrawPixel((uint16_t)(x + col), (uint16_t)(y + row), bg);
            }
        }
    }
}

void GFX_DrawText8x16(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    if (text == NULL)
    {
        return;
    }

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + 16U);
        }
        else
        {
            GFX_DrawChar8x16(cursor_x, cursor_y, *text, fg, bg);
            cursor_x = (uint16_t)(cursor_x + 8U);
        }
        ++text;
    }
}

