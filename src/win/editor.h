#ifndef MINUTA_EDITOR_H
#define MINUTA_EDITOR_H

#include <windows.h>

/* Loads RICHED20.DLL and creates the editor control. NULL on failure. */
HWND editor_create(HWND parent);

/* Applies font, colors and paragraph spacing from app.settings/app.theme. */
void editor_apply_settings(void);

/* Centers the text column in the control. Call after resizing it. */
void editor_layout(void);

/* Replaces the whole text (CP1252, "\r\n" line breaks) and clears undo. */
int editor_set_text(const char *text, size_t size);

/* Returns the whole text with "\r\n" line breaks (malloc'ed). */
char *editor_get_text(size_t *size);

/* Returns the text with RichEdit's own "\r" paragraph breaks, so offsets
 * match the control's character positions (malloc'ed). */
char *editor_get_raw_text(size_t *size);

/* Replaces [start, end) with text (used by undo), leaving the caret after
 * it, and updates styling and the modified state. */
void editor_replace(long start, long end, const char *text, long size);

/* Call on every EN_CHANGE from the editor. */
void editor_notify_change(void);

#endif
