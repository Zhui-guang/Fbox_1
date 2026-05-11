#include "ui_widgets.h"
#include "screen.h"
#include "ui_layout.h"
#include <stdio.h>
#include <string.h>

void UIW_DrawStatusBar(const char *left_text, const char *right_text)
{
    Screen_FillRect(UI_SAFE_X, 0U, UI_SAFE_W, UI_TOP_H, 0x10A2U);
    Screen_FillRect(UI_SAFE_X, (uint16_t)(UI_TOP_H - 1U), UI_SAFE_W, 1U, UI_ACCENT_COLOR);
    if (left_text != (const char *)0)
    {
        Screen_DrawTextUtf8Fallback(UI_HEADER_TEXT_X, (uint16_t)(UI_SAFE_Y + 1U), left_text, 0xFFFFU, 0x10A2U);
    }
    if (right_text != (const char *)0)
    {
        uint16_t x = (uint16_t)(UI_SCREEN_W - UI_MARGIN_X - UI_EDGE_GUARD_CH_W - (uint16_t)(8U * (uint16_t)strlen(right_text)));
        Screen_DrawTextUtf8Fallback(x, (uint16_t)(UI_SAFE_Y + 1U), right_text, UI_ACCENT_COLOR, 0x10A2U);
    }
}

void UIW_DrawList(const char *title, const char *const *items, uint8_t item_count, uint8_t selected, const char *footer_hint)
{
    uint8_t i;
    char line_buf[32];

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader(title);
    for (i = 0U; i < item_count; ++i)
    {
        uint16_t y = (uint16_t)((3U + i) * UI_LINE_HEIGHT);
        uint16_t row_w = (uint16_t)(UI_SAFE_W - 8U);
        uint16_t card_bg = (i == selected) ? UI_HIGHLIGHT_COLOR : 0x10A2U;
        uint16_t fg = (i == selected) ? UI_HIGHLIGHT_TEXT : UI_FG_COLOR;

        Screen_FillRect(UI_MARGIN_X, y, row_w, UI_LINE_HEIGHT, 0x0000U); /* shadow */
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(y - 1U), row_w, UI_LINE_HEIGHT, card_bg);
        Screen_FillRect(UI_MARGIN_X, (uint16_t)(y - 1U), 3U, UI_LINE_HEIGHT, UI_ACCENT_COLOR);

        if (i == selected)
        {
            (void)snprintf(line_buf, sizeof(line_buf), "> %s", items[i]);
            Screen_DrawTextUtf8Fallback((uint16_t)(UI_MARGIN_X + 4U), (uint16_t)(y - 1U), line_buf, fg, card_bg);
        }
        else
        {
            (void)snprintf(line_buf, sizeof(line_buf), "  %s", items[i]);
            Screen_DrawTextUtf8Fallback((uint16_t)(UI_MARGIN_X + 4U), (uint16_t)(y - 1U), line_buf, fg, card_bg);
        }
    }
    UI_Layout_DrawFooter(footer_hint);
}

void UIW_DrawDialog(const char *title, const char *line1, const char *line2, const char *footer_hint)
{
    uint16_t box_x = UI_MARGIN_X;
    uint16_t box_y = (uint16_t)(UI_TOP_H + 28U);
    uint16_t box_w = (uint16_t)(UI_SAFE_W - 8U);
    uint16_t box_h = 120U;

    UI_Layout_ClearScreen();
    UI_Layout_DrawHeader(title);
    Screen_FillRect(box_x, box_y, box_w, box_h, 0x0000U);
    Screen_FillRect((uint16_t)(box_x + 2U), (uint16_t)(box_y + 2U), (uint16_t)(box_w - 4U), (uint16_t)(box_h - 4U), UI_BG_COLOR);
    Screen_FillRect((uint16_t)(box_x + 2U), (uint16_t)(box_y + 2U), (uint16_t)(box_w - 4U), 2U, UI_ACCENT_COLOR);
    if (line1 != (const char *)0)
    {
        Screen_DrawTextUtf8Fallback((uint16_t)(box_x + 10U), (uint16_t)(box_y + 24U), line1, UI_FG_COLOR, UI_BG_COLOR);
    }
    if (line2 != (const char *)0)
    {
        Screen_DrawTextUtf8Fallback((uint16_t)(box_x + 10U), (uint16_t)(box_y + 44U), line2, UI_FG_COLOR, UI_BG_COLOR);
    }
    UI_Layout_DrawFooter(footer_hint);
}
