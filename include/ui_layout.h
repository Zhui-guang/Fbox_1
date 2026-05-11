#ifndef __UI_LAYOUT_H__
#define __UI_LAYOUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define UI_SCREEN_W          240U
#define UI_SCREEN_H          280U
#define UI_TOP_H             22U
#define UI_BOTTOM_H          18U
#define UI_CONTENT_X         0U
#define UI_CONTENT_Y         UI_TOP_H
#define UI_CONTENT_W         UI_SCREEN_W
#define UI_CONTENT_H         (UI_SCREEN_H - UI_TOP_H - UI_BOTTOM_H)
#define UI_SAFE_MARGIN_X     8U
#define UI_SAFE_MARGIN_Y     4U
#define UI_SAFE_X            UI_SAFE_MARGIN_X
#define UI_SAFE_Y            UI_SAFE_MARGIN_Y
#define UI_SAFE_W            (UI_SCREEN_W - (UI_SAFE_MARGIN_X * 2U))
#define UI_SAFE_H            (UI_SCREEN_H - (UI_SAFE_MARGIN_Y * 2U))

#define UI_BG_COLOR          0x0841U
#define UI_FG_COLOR          0xFFFFU
#define UI_ACCENT_COLOR      0x2D7FU
#define UI_HIGHLIGHT_COLOR   0xDF1BU
#define UI_HIGHLIGHT_TEXT    0x0000U
#define UI_WARN_COLOR        0xFD20U
#define UI_MARGIN_X          (UI_SAFE_MARGIN_X + 4U)
#define UI_LINE_HEIGHT       16U
#define UI_EDGE_GUARD_CH_W   8U
#define UI_HEADER_TEXT_X     (UI_MARGIN_X + UI_EDGE_GUARD_CH_W)
#define UI_FOOTER_TEXT_X     (UI_MARGIN_X + UI_EDGE_GUARD_CH_W)

void UI_Layout_ClearScreen(void);
void UI_Layout_DrawHeader(const char *title);
void UI_Layout_ClearContent(void);
void UI_Layout_DrawFooter(const char *hint);
void UI_Layout_DrawTextLine(uint16_t x, uint16_t line, const char *text, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* __UI_LAYOUT_H__ */
