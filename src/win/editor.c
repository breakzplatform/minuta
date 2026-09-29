#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#include "app.h"
#include "buffer.h"
#include "editor.h"
#include "style.h"
#include "trace.h"
#include "undo.h"

/* Enough for a few MB of text; RichEdit's default typing limit is 32K. */
#define EDITOR_TEXT_LIMIT (16L * 1024 * 1024)

static WNDPROC richedit_procedure;

/* Edit tracking: every message that can change the text goes through
 * tracked_edit, which works out which range changed and tells style.c. */
static int tracking;
static int changes_during_tracking;
static int changes_expected_later;

struct stream_source {
    const char *data;
    size_t size;
    size_t position;
};

static DWORD CALLBACK read_stream(DWORD_PTR cookie, LPBYTE destination, LONG wanted, LONG *copied)
{
    struct stream_source *source = (struct stream_source *)cookie;
    size_t left = source->size - source->position;
    size_t count = (size_t)wanted < left ? (size_t)wanted : left;

    memcpy(destination, source->data + source->position, count);
    source->position += count;
    *copied = (LONG)count;
    return 0;
}

static DWORD CALLBACK write_stream(DWORD_PTR cookie, LPBYTE data, LONG size, LONG *written)
{
    struct buffer *out = (struct buffer *)cookie;

    buffer_append(out, (const char *)data, (size_t)size);
    *written = size;
    return out->failed ? 1 : 0;
}

static long text_length(void)
{
    GETTEXTLENGTHEX query;

    query.flags = GTL_NUMCHARS;
    query.codepage = CP_ACP;
    return (long)SendMessage(app.editor, EM_GETTEXTLENGTHEX, (WPARAM)&query, 0);
}

void editor_notify_change(void)
{
    if (tracking > 0) {
        changes_during_tracking++;
    } else if (changes_expected_later > 0) {
        changes_expected_later--;
    } else if (!style_busy) {
        /* A change we didn't see coming (drag and drop): start over. */
        app_document_changed();
        style_reset();
    }
}

static int is_edit_message(UINT message, WPARAM wparam)
{
    switch (message) {
    case WM_CHAR:
    case WM_IME_CHAR:
    case WM_PASTE:
    case WM_CUT:
    case WM_CLEAR:
    case EM_REPLACESEL:
    case EM_PASTESPECIAL:
    case EM_UNDO:
    case EM_REDO:
        return 1;
    case WM_KEYDOWN:
        return wparam == VK_DELETE || wparam == VK_BACK
            || (GetKeyState(VK_CONTROL) < 0 && (wparam == 'X' || wparam == 'Z' || wparam == 'Y'));
    default:
        return 0;
    }
}

/* Characters kept around the selection before an edit, enough to recover
 * what Backspace, Delete and their Ctrl+ word variants remove. */
#define UNDO_CONTEXT 256

static enum undo_kind edit_kind(UINT message, WPARAM wparam)
{
    if (message == WM_CHAR) {
        if (wparam == '\b') {
            return UNDO_BACKSPACE;
        }
        return wparam >= ' ' && wparam != 0x7F ? UNDO_TYPING : UNDO_OTHER;
    }
    if (message == WM_KEYDOWN && wparam == VK_DELETE && GetKeyState(VK_CONTROL) >= 0) {
        return UNDO_DELETE;
    }
    return UNDO_OTHER;
}

static char *range_text(long start, long end)
{
    TEXTRANGE range;
    char *text = malloc((size_t)(end - start) + 1);

    if (text == NULL) {
        return NULL;
    }
    range.chrg.cpMin = start;
    range.chrg.cpMax = end;
    range.lpstrText = text;
    text[end > start ? SendMessage(app.editor, EM_GETTEXTRANGE, 0, (LPARAM)&range) : 0] = '\0';
    return text;
}

static void record_undo(long context_start, const char *context, long context_end, long start,
                        long removed, long inserted, enum undo_kind kind)
{
    char *inserted_text;

    if (context == NULL || start < context_start || start + removed > context_end) {
        undo_clear();
        return;
    }
    inserted_text = range_text(start, start + inserted);
    if (inserted_text == NULL) {
        undo_clear();
        return;
    }
    undo_record(start, context + (start - context_start), removed, inserted_text, inserted, kind);
    free(inserted_text);
}

static LRESULT tracked_edit(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    CHARRANGE before;
    CHARRANGE after;
    long length_before = text_length();
    long context_start;
    long context_end;
    char *context;
    BOOL was_modified = (BOOL)SendMessage(window, EM_GETMODIFY, 0, 0);
    int outer_changes = changes_during_tracking;
    /* Undo and redo can change text anywhere. */
    int undo_or_redo = message == EM_UNDO || message == EM_REDO
        || (message == WM_KEYDOWN && (wparam == 'Z' || wparam == 'Y'));
    int changed;
    LRESULT result;

    SendMessage(window, EM_EXGETSEL, 0, (LPARAM)&before);
    context_start = before.cpMin > UNDO_CONTEXT ? before.cpMin - UNDO_CONTEXT : 0;
    context_end = before.cpMax + UNDO_CONTEXT < length_before ? before.cpMax + UNDO_CONTEXT : length_before;
    context = range_text(context_start, context_end);
    SendMessage(window, EM_SETMODIFY, FALSE, 0);
    changes_during_tracking = 0;
    tracking++;
    result = CallWindowProc(richedit_procedure, window, message, wparam, lparam);
    tracking--;
    changed = (int)SendMessage(window, EM_GETMODIFY, 0, 0);
    SendMessage(window, EM_SETMODIFY, was_modified || changed, 0);
    if (changed) {
        long delta = text_length() - length_before;
        long start;
        long inserted;

        if (changes_during_tracking == 0) {
            changes_expected_later++;
        }
        SendMessage(window, EM_EXGETSEL, 0, (LPARAM)&after);
        /* Typing, pasting and deleting leave the caret after the new text:
         * the edit replaced [start, start + removed) by [start, caret). */
        start = before.cpMin < after.cpMax ? before.cpMin : after.cpMax;
        inserted = after.cpMax - start;
        app_document_changed();
        if (undo_or_redo || inserted - delta < 0) {
            undo_clear();
            style_reset();
        } else {
            record_undo(context_start, context, context_end, start, inserted - delta, inserted,
                        edit_kind(message, wparam));
            style_edit(start, inserted - delta, inserted);
        }
    }
    free(context);
    changes_during_tracking = outer_changes + changes_during_tracking;
    return result;
}

void editor_replace(long start, long end, const char *text, long size)
{
    CHARRANGE range;

    range.cpMin = start;
    range.cpMax = end;
    tracking++;
    SendMessage(app.editor, EM_EXSETSEL, 0, (LPARAM)&range);
    CallWindowProc(richedit_procedure, app.editor, EM_REPLACESEL, FALSE, (LPARAM)text);
    tracking--;
    changes_expected_later++;
    range.cpMin = start + size;
    range.cpMax = start + size;
    SendMessage(app.editor, EM_EXSETSEL, 0, (LPARAM)&range);
    app_document_changed();
    style_edit(start, end - start, size);
}

static int is_view_message(UINT message)
{
    return message == WM_VSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN
        || message == WM_LBUTTONUP || message == WM_SIZE;
}

static LRESULT CALLBACK editor_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result;

    if (message == WM_KEYDOWN) {
        int control = GetKeyState(VK_CONTROL) < 0;
        int shift = GetKeyState(VK_SHIFT) < 0;

        /* Paste plain text only, so no foreign formatting gets in. */
        if ((control && wparam == 'V') || (shift && wparam == VK_INSERT)) {
            return tracked_edit(window, EM_PASTESPECIAL, CF_TEXT, 0);
        }
        /* No overtype mode. */
        if (wparam == VK_INSERT && !control && !shift) {
            return 0;
        }
    }
    if (is_edit_message(message, wparam)) {
        return tracked_edit(window, message, wparam, lparam);
    }
    result = CallWindowProc(richedit_procedure, window, message, wparam, lparam);
    if (is_view_message(message) && !style_busy) {
        style_view_changed();
    }
    return result;
}

HWND editor_create(HWND parent)
{
    HWND editor;

    if (LoadLibrary("RICHED20.DLL") == NULL) {
        return NULL;
    }
    editor = CreateWindowEx(0, RICHEDIT_CLASSA, "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL
                                | ES_NOHIDESEL | ES_SAVESEL | ES_DISABLENOSCROLL,
                            0, 0, 0, 0, parent, NULL, app.instance, NULL);
    if (editor == NULL) {
        return NULL;
    }
    SendMessage(editor, EM_EXLIMITTEXT, 0, EDITOR_TEXT_LIMIT);
    /* Undo lives in undo.c; RichEdit's would also undo styling. */
    SendMessage(editor, EM_SETUNDOLIMIT, 0, 0);
    /* No automatic font switching: everything stays in the editor font. */
    SendMessage(editor, EM_SETLANGOPTIONS, 0, 0);
    SendMessage(editor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL);
    richedit_procedure = (WNDPROC)SetWindowLong(editor, GWL_WNDPROC, (LONG)editor_procedure);
    return editor;
}

static void base_format(CHARFORMAT2 *format)
{
    memset(format, 0, sizeof(*format));
    format->cbSize = sizeof(*format);
    format->dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE
                     | CFM_STRIKEOUT | CFM_BACKCOLOR | CFM_CHARSET;
    format->dwEffects = CFE_AUTOBACKCOLOR;
    format->yHeight = app.settings.font_size * 20;
    format->crTextColor = app.theme.text;
    format->bCharSet = ANSI_CHARSET;
    format->bPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(format->szFaceName, app.settings.font_name, LF_FACESIZE - 1);
}

void editor_apply_settings(void)
{
    CHARFORMAT2 format;
    BOOL modified = (BOOL)SendMessage(app.editor, EM_GETMODIFY, 0, 0);
    /* Formatting changes send EN_CHANGE too; they are not edits. */
    LRESULT event_mask = SendMessage(app.editor, EM_SETEVENTMASK, 0, 0);

    SendMessage(app.editor, WM_SETREDRAW, FALSE, 0);
    SendMessage(app.editor, EM_SETBKGNDCOLOR, 0, (LPARAM)app.theme.background);
    base_format(&format);
    SendMessage(app.editor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
    SendMessage(app.editor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    SendMessage(app.editor, EM_SETMODIFY, modified, 0);
    SendMessage(app.editor, EM_SETEVENTMASK, 0, event_mask);
    SendMessage(app.editor, WM_SETREDRAW, TRUE, 0);
    editor_layout();
    InvalidateRect(app.editor, NULL, TRUE);
}

/* Width of one character of the editor font, in pixels. */
static int character_width(void)
{
    HDC dc = GetDC(app.editor);
    HFONT font = CreateFont(-MulDiv(app.settings.font_size, GetDeviceCaps(dc, LOGPIXELSY), 72),
                            0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
                            app.settings.font_name);
    HGDIOBJ previous = SelectObject(dc, font);
    SIZE extent;
    int width;

    GetTextExtentPoint32(dc, "MMMMMMMMMM", 10, &extent);
    width = (extent.cx + 9) / 10;
    SelectObject(dc, previous);
    DeleteObject(font);
    ReleaseDC(app.editor, dc);
    return width > 0 ? width : 8;
}

void editor_layout(void)
{
    RECT client;
    RECT column;
    int width;
    int margin;

    GetClientRect(app.editor, &client);
    /* One extra character so RichEdit's caret never forces an early wrap. */
    width = (app.settings.column_width + 1) * character_width();
    margin = (client.right - width) / 2;
    if (margin < 12) {
        margin = 12;
    }
    column.left = margin;
    column.right = client.right - margin;
    column.top = client.bottom > 200 ? 32 : 4;
    column.bottom = client.bottom;
    if (column.right <= column.left) {
        column.right = column.left + 1;
    }
    SendMessage(app.editor, EM_SETRECT, 0, (LPARAM)&column);
}

int editor_set_text(const char *text, size_t size)
{
    struct stream_source source;
    EDITSTREAM stream;
    CHARFORMAT2 format;
    LRESULT event_mask;

    source.data = text;
    source.size = size;
    source.position = 0;
    stream.dwCookie = (DWORD_PTR)&source;
    stream.dwError = 0;
    stream.pfnCallback = read_stream;
    SendMessage(app.editor, WM_SETREDRAW, FALSE, 0);
    event_mask = SendMessage(app.editor, EM_SETEVENTMASK, 0, 0);
    /* Measured with a 1 MB file on Windows 98: replacing all text is quick
     * (35 ms), and so is one character format for everything; streaming into
     * a selection or giving every paragraph a format makes RichEdit lay out
     * all lines at once (3.5 s). So the line spacing is left to style.c,
     * which sets it paragraph by paragraph as they get styled. */
    SendMessage(app.editor, EM_STREAMIN, SF_TEXT, (LPARAM)&stream);
    base_format(&format);
    SendMessage(app.editor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
    trace("streamed in");
    SendMessage(app.editor, EM_SETEVENTMASK, 0, event_mask);
    SendMessage(app.editor, EM_EMPTYUNDOBUFFER, 0, 0);
    undo_clear();
    SendMessage(app.editor, EM_SETSEL, 0, 0);
    SendMessage(app.editor, EM_SETMODIFY, FALSE, 0);
    SendMessage(app.editor, WM_SETREDRAW, TRUE, 0);
    SendMessage(app.editor, EM_SETBKGNDCOLOR, 0, (LPARAM)app.theme.background);
    editor_layout();
    InvalidateRect(app.editor, NULL, TRUE);
    style_reset();
    trace("styled");
    return stream.dwError == 0;
}

char *editor_get_text(size_t *size)
{
    struct buffer out;
    EDITSTREAM stream;

    buffer_init(&out);
    stream.dwCookie = (DWORD_PTR)&out;
    stream.dwError = 0;
    stream.pfnCallback = write_stream;
    SendMessage(app.editor, EM_STREAMOUT, SF_TEXT, (LPARAM)&stream);
    if (stream.dwError != 0) {
        buffer_free(&out);
        return NULL;
    }
    return buffer_take(&out, size);
}

char *editor_get_raw_text(size_t *size)
{
    /* In chunks: one EM_GETTEXTRANGE over a 1 MB document returned nothing
     * on Windows 98. */
    enum { CHUNK = 32 * 1024 };
    long length = text_length();
    long position = 0;
    char *text = malloc((size_t)length + 1);

    if (text == NULL) {
        return NULL;
    }
    while (position < length) {
        TEXTRANGE range;
        long count;

        range.chrg.cpMin = position;
        range.chrg.cpMax = position + CHUNK < length ? position + CHUNK : length;
        range.lpstrText = text + position;
        count = (long)SendMessage(app.editor, EM_GETTEXTRANGE, 0, (LPARAM)&range);
        if (count <= 0) {
            break;
        }
        position += count;
    }
    text[position] = '\0';
    *size = (size_t)position;
    return text;
}
