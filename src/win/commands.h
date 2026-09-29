#ifndef MINUTA_COMMANDS_H
#define MINUTA_COMMANDS_H

#include <windows.h>

/* Export HTML (Ctrl+E), the word count overlay (Ctrl+Shift+W) and
 * Find/Replace (Ctrl+F, F3, Ctrl+H). */

void export_document(void);

void counters_show(void);

void find_open(int replace);
void find_next(void);
/* The modeless Find/Replace dialog, or NULL, for IsDialogMessage. */
HWND find_dialog(void);
/* The message the Find/Replace dialog sends to the main window. */
UINT find_message(void);
void find_handle(LPARAM lparam);

#endif
