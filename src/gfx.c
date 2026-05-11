#include "gfx.h"
#include "font_ascii_1608.h"
#include "font_zh16.h"
#include "st7789.h"

static uint16_t gfx_utf8_decode(const uint8_t *p, uint8_t *advance)
{
    if ((p[0] & 0x80U) == 0U)
    {
        *advance = 1U;
        return p[0];
    }
    if ((p[0] & 0xE0U) == 0xC0U)
    {
        *advance = 2U;
        return (uint16_t)(((p[0] & 0x1FU) << 6) | (p[1] & 0x3FU));
    }
    if ((p[0] & 0xF0U) == 0xE0U)
    {
        *advance = 3U;
        return (uint16_t)(((p[0] & 0x0FU) << 12) | ((p[1] & 0x3FU) << 6) | (p[2] & 0x3FU));
    }
    *advance = 1U;
    return '?';
}

static void gfx_draw_zh16(uint16_t x, uint16_t y, const FontZh16Glyph *g, uint16_t fg, uint16_t bg)
{
    uint16_t px[16U * 16U];
    uint8_t row;
    uint8_t col;
    uint16_t i = 0U;

    for (row = 0U; row < 16U; ++row)
    {
        uint8_t b0 = g->rows[row * 2U];
        uint8_t b1 = g->rows[row * 2U + 1U];
        for (col = 0U; col < 8U; ++col)
        {
            px[i++] = ((b0 & (uint8_t)(1U << (7U - col))) != 0U) ? fg : bg;
        }
        for (col = 0U; col < 8U; ++col)
        {
            px[i++] = ((b1 & (uint8_t)(1U << (7U - col))) != 0U) ? fg : bg;
        }
    }
    ST7789_DrawRGB565Bitmap(x, y, 16U, 16U, px);
}

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

void GFX_DrawRGB565Bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels)
{
    ST7789_DrawRGB565Bitmap(x, y, w, h, pixels);
}

void GFX_DrawChar8x16(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg)
{
    uint8_t row;
    uint8_t col;
    const uint8_t *glyph;
    uint16_t px_buf[8U * 16U];
    uint32_t idx = 0U;

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
                px_buf[idx++] = fg;
            }
            else
            {
                px_buf[idx++] = bg;
            }
        }
    }
    ST7789_DrawRGB565Bitmap(x, y, 8U, 16U, px_buf);
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

void GFX_DrawTextUtf8Fallback(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg)
{
    uint16_t cx = x;
    const uint8_t *p = (const uint8_t *)text;

    if (text == NULL)
    {
        return;
    }

    while (*p != 0U)
    {
        if (*p < 0x80U)
        {
            GFX_DrawChar8x16(cx, y, (char)(*p), fg, bg);
            cx = (uint16_t)(cx + 8U);
            p++;
        }
        else
        {
            uint8_t adv = 1U;
            uint16_t code = gfx_utf8_decode(p, &adv);
            const FontZh16Glyph *zh = FontZh16_Find(code);
            if (zh != (const FontZh16Glyph *)0)
            {
                gfx_draw_zh16(cx, y, zh, fg, bg);
            }
            else
            {
                /* Unknown non-ASCII glyph fallback box */
                GFX_FillRect(cx, y, 16U, 16U, bg);
                GFX_FillRect(cx, y, 16U, 1U, fg);
                GFX_FillRect(cx, (uint16_t)(y + 15U), 16U, 1U, fg);
                GFX_FillRect(cx, y, 1U, 16U, fg);
                GFX_FillRect((uint16_t)(cx + 15U), y, 1U, 16U, fg);
            }
            p += adv;
            cx = (uint16_t)(cx + 16U);
        }
    }
}
