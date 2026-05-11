#ifndef __UI_WIDGETS_H__
#define __UI_WIDGETS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void UIW_DrawStatusBar(const char *left_text, const char *right_text);
void UIW_DrawList(const char *title, const char *const *items, uint8_t item_count, uint8_t selected, const char *footer_hint);
void UIW_DrawDialog(const char *title, const char *line1, const char *line2, const char *footer_hint);

#ifdef __cplusplus
}
#endif

#endif /* __UI_WIDGETS_H__ */

