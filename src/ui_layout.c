#include "ui_layout.h"
#include "screen.h"

void UI_Layout_ClearScreen(void)
{
    Screen_Clear(UI_BG_COLOR);
}

void UI_Layout_DrawHeader(const char *title)
{
    Screen_FillRect(UI_SAFE_X, 0U, UI_SAFE_W, UI_TOP_H, UI_BG_COLOR);
    Screen_DrawText(UI_HEADER_TEXT_X, (uint16_t)(UI_SAFE_Y + 1U), title, UI_ACCENT_COLOR, UI_BG_COLOR);
}

void UI_Layout_ClearContent(void)
{
    Screen_FillRect(UI_CONTENT_X, UI_CONTENT_Y, UI_CONTENT_W, UI_CONTENT_H, UI_BG_COLOR);
}

void UI_Layout_DrawFooter(const char *hint)
{
    Screen_FillRect(UI_SAFE_X, UI_SCREEN_H - UI_BOTTOM_H, UI_SAFE_W, UI_BOTTOM_H, UI_BG_COLOR);
    if (hint != (const char *)0)
    {
        Screen_DrawText(UI_FOOTER_TEXT_X, (uint16_t)(UI_SCREEN_H - UI_BOTTOM_H + 1U), hint, UI_FG_COLOR, UI_BG_COLOR);
    }
}

void UI_Layout_DrawTextLine(uint16_t x, uint16_t line, const char *text, uint16_t color)
{
    Screen_DrawText(x, (uint16_t)(line * UI_LINE_HEIGHT), text, color, UI_BG_COLOR);
}
