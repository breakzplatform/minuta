#ifndef MINUTA_PREVIEW_H
#define MINUTA_PREVIEW_H

#include <windows.h>

/* The read-only RichEdit that shows the rendered document (Ctrl+R). */

HWND preview_create(HWND parent);

/* Renders the document and shows it in place of the editor, at the
 * position of the editor's caret. */
void preview_show(void);

/* Goes back to the editor. If the preview was scrolled or clicked, the
 * editor caret moves to the matching place; otherwise it stays put. */
void preview_hide(void);

/* Colors and column after a settings change. */
void preview_apply_settings(void);

#endif
