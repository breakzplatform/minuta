#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <commdlg.h>
#include <richedit.h>

#include "app.h"
#include "buffer.h"
#include "commands.h"
#include "editor.h"
#include "export.h"
#include "files.h"
#include "stats.h"

/* ---- Export HTML ---- */

typedef HINSTANCE (WINAPI *shell_execute_function)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);

/* SHELL32 is loaded only here, to keep the imports to what the spec allows. */
static void open_in_browser(const char *path)
{
    HMODULE shell = LoadLibrary("SHELL32.DLL");
    shell_execute_function shell_execute;

    if (shell == NULL) {
        return;
    }
    shell_execute = (shell_execute_function)GetProcAddress(shell, "ShellExecuteA");
    if (shell_execute == NULL
        || (INT_PTR)shell_execute(app.window, "open", path, NULL, NULL, SW_SHOWNORMAL) <= 32) {
        app_error("Can't open %s in a browser.", path);
    }
}

/* The document's file name with its extension replaced by ".htm". */
static void default_export_path(char *path, size_t size)
{
    const char *name = app.path[0] != '\0' ? app.path : "Untitled.md";
    char *dot;
    char *slash;

    strncpy(path, name, size - 5);
    path[size - 5] = '\0';
    dot = strrchr(path, '.');
    slash = strrchr(path, '\\');
    if (dot != NULL && (slash == NULL || dot > slash)) {
        *dot = '\0';
    }
    strcat(path, ".htm");
}

void export_document(void)
{
    static const char filter[] = "Web page (*.htm;*.html)\0*.htm;*.html\0";
    OPENFILENAME dialog;
    char path[MAX_PATH];
    char title[MAX_PATH];
    const char *name;
    struct buffer html;
    size_t text_size;
    size_t bytes_size;
    size_t markdown_size;
    char *text;
    char *bytes;
    char *markdown;
    int ok;

    default_export_path(path, sizeof(path));
    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrDefExt = "htm";
    dialog.lpstrTitle = "Export HTML";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileName(&dialog)) {
        return;
    }
    /* The saved form of the document, so characters outside CP1252 in a
     * UTF-8 file reach the page instead of their '?' placeholders. */
    text = editor_get_text(&text_size);
    bytes = text != NULL ? document_encode(text, text_size, &app.format, &bytes_size) : NULL;
    free(text);
    if (bytes == NULL) {
        app_error("Not enough memory to export %s.", path);
        return;
    }
    if (app.format.encoding == ENCODING_CP1252) {
        markdown = encoding_to_utf8(bytes, bytes_size, ENCODING_CP1252, &markdown_size);
        free(bytes);
    } else {
        markdown = bytes;
        markdown_size = bytes_size;
    }
    name = app.path[0] != '\0' ? strrchr(app.path, '\\') : NULL;
    strcpy(title, name != NULL ? name + 1 : (app.path[0] != '\0' ? app.path : "Untitled"));
    buffer_init(&html);
    ok = markdown != NULL && export_html(markdown, markdown_size, title, app.format.encoding, &html) == 0
         && files_write(path, html.data, html.size);
    free(markdown);
    buffer_free(&html);
    if (!ok) {
        app_error("Can't export %s.", path);
        return;
    }
    if (MessageBox(app.window, "Exported. Open the page in your web browser?", APP_NAME,
                   MB_YESNO | MB_ICONQUESTION) == IDYES) {
        open_in_browser(path);
    }
}

/* ---- Word count overlay ---- */

#define OVERLAY_CLASS "MinutaOverlay"
#define OVERLAY_TIMER 1
#define OVERLAY_MILLISECONDS 4000

static HWND overlay;
static char overlay_text[128];

static LRESULT CALLBACK overlay_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        RECT area;
        HDC dc = BeginPaint(window, &paint);
        HBRUSH brush = CreateSolidBrush(app.theme.code_background);
        HGDIOBJ previous = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

        GetClientRect(window, &area);
        FillRect(dc, &area, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, app.theme.text);
        DrawText(dc, overlay_text, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, previous);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_TIMER:
        KillTimer(window, OVERLAY_TIMER);
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_LBUTTONDOWN:
        ShowWindow(window, SW_HIDE);
        return 0;
    default:
        return DefWindowProc(window, message, wparam, lparam);
    }
}

void counters_show(void)
{
    struct text_stats stats;
    size_t size;
    char *text = editor_get_raw_text(&size);
    RECT client;
    POINT corner;
    SIZE extent;
    HDC dc;
    HGDIOBJ previous;
    int width;
    int height;

    if (text == NULL) {
        return;
    }
    text_stats_count(text, size, &stats);
    free(text);
    /* "\xB7" is the middle dot. */
    sprintf(overlay_text, "%lu words  \xB7  %lu characters  \xB7  %lu min read", stats.words,
            stats.characters, stats.reading_minutes);
    if (overlay == NULL) {
        WNDCLASS overlay_class;

        memset(&overlay_class, 0, sizeof(overlay_class));
        overlay_class.lpfnWndProc = overlay_procedure;
        overlay_class.hInstance = app.instance;
        overlay_class.hCursor = LoadCursor(NULL, IDC_ARROW);
        overlay_class.lpszClassName = OVERLAY_CLASS;
        RegisterClass(&overlay_class);
        overlay = CreateWindowEx(WS_EX_TOOLWINDOW, OVERLAY_CLASS, "", WS_POPUP | WS_BORDER, 0, 0, 0, 0,
                                 app.window, NULL, app.instance, NULL);
        if (overlay == NULL) {
            return;
        }
    }
    dc = GetDC(overlay);
    previous = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    GetTextExtentPoint32(dc, overlay_text, (int)strlen(overlay_text), &extent);
    SelectObject(dc, previous);
    ReleaseDC(overlay, dc);
    width = extent.cx + 40;
    height = extent.cy + 16;
    GetClientRect(app.window, &client);
    corner.x = (client.right - width) / 2;
    corner.y = client.bottom - height - 24;
    ClientToScreen(app.window, &corner);
    SetWindowPos(overlay, HWND_TOP, corner.x, corner.y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(overlay, NULL, TRUE);
    SetTimer(overlay, OVERLAY_TIMER, OVERLAY_MILLISECONDS, NULL);
}

/* ---- Find and Replace ---- */

static FINDREPLACE find_state;
static HWND find_window;
static UINT find_message_id;
static char find_what[256];
static char replace_with[256];

UINT find_message(void)
{
    if (find_message_id == 0) {
        find_message_id = RegisterWindowMessage(FINDMSGSTRING);
    }
    return find_message_id;
}

HWND find_dialog(void)
{
    return find_window;
}

void find_open(int replace)
{
    CHARRANGE selection;

    if (find_window != NULL) {
        DestroyWindow(find_window);
        find_window = NULL;
    }
    /* Start from the selected text, when it is a short single line. */
    SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&selection);
    if (selection.cpMax > selection.cpMin && selection.cpMax - selection.cpMin < (long)sizeof(find_what)) {
        char text[sizeof(find_what)];

        SendMessage(app.editor, EM_GETSELTEXT, 0, (LPARAM)text);
        if (strchr(text, '\r') == NULL) {
            strcpy(find_what, text);
        }
    }
    memset(&find_state, 0, sizeof(find_state));
    find_state.lStructSize = sizeof(find_state);
    find_state.hwndOwner = app.window;
    find_state.lpstrFindWhat = find_what;
    find_state.wFindWhatLen = sizeof(find_what);
    find_state.lpstrReplaceWith = replace_with;
    find_state.wReplaceWithLen = sizeof(replace_with);
    find_state.Flags = FR_DOWN;
    find_message();
    find_window = replace ? ReplaceText(&find_state) : FindText(&find_state);
}

/* Searches from the selection; selects and returns 1 on a match. */
static int search(DWORD flags, int quiet)
{
    FINDTEXTEX query;
    CHARRANGE selection;
    WPARAM options = 0;

    SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&selection);
    if (flags & FR_MATCHCASE) {
        options |= FR_MATCHCASE;
    }
    if (flags & FR_WHOLEWORD) {
        options |= FR_WHOLEWORD;
    }
    if (flags & FR_DOWN) {
        options |= FR_DOWN;
        query.chrg.cpMin = selection.cpMax;
        query.chrg.cpMax = -1;
    } else {
        query.chrg.cpMin = selection.cpMin;
        query.chrg.cpMax = 0;
    }
    query.lpstrText = find_what;
    if (SendMessage(app.editor, EM_FINDTEXTEX, options, (LPARAM)&query) < 0) {
        if (!quiet) {
            app_error("Cannot find \"%s\".", find_what);
        }
        return 0;
    }
    SendMessage(app.editor, EM_EXSETSEL, 0, (LPARAM)&query.chrgText);
    SendMessage(app.editor, EM_SCROLLCARET, 0, 0);
    return 1;
}

void find_next(void)
{
    if (find_what[0] == '\0') {
        find_open(0);
        return;
    }
    search(find_state.lStructSize != 0 ? find_state.Flags | FR_DOWN : FR_DOWN, 0);
}

static int selection_matches(DWORD flags)
{
    CHARRANGE selection;
    char text[sizeof(find_what)];
    long length = (long)strlen(find_what);

    SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&selection);
    if (selection.cpMax - selection.cpMin != length) {
        return 0;
    }
    SendMessage(app.editor, EM_GETSELTEXT, 0, (LPARAM)text);
    return (flags & FR_MATCHCASE) ? strcmp(text, find_what) == 0 : lstrcmpi(text, find_what) == 0;
}

void find_handle(LPARAM lparam)
{
    FINDREPLACE *request = (FINDREPLACE *)lparam;
    DWORD flags = request->Flags;

    if (flags & FR_DIALOGTERM) {
        find_window = NULL;
        return;
    }
    if (flags & FR_FINDNEXT) {
        search(flags, 0);
    } else if (flags & FR_REPLACE) {
        if (selection_matches(flags)) {
            editor_replace_selection(replace_with);
        }
        search(flags | FR_DOWN, 0);
    } else if (flags & FR_REPLACEALL) {
        CHARRANGE start;
        char message[64];
        int count = 0;

        start.cpMin = 0;
        start.cpMax = 0;
        SendMessage(app.editor, EM_EXSETSEL, 0, (LPARAM)&start);
        while (search(flags | FR_DOWN, 1)) {
            editor_replace_selection(replace_with);
            count++;
        }
        sprintf(message, "Replaced %d occurrence%s.", count, count == 1 ? "" : "s");
        MessageBox(find_window != NULL ? find_window : app.window, message, APP_NAME, MB_OK | MB_ICONINFORMATION);
    }
}
