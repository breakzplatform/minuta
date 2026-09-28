#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#include "app.h"
#include "buffer.h"
#include "editor.h"

/* Enough for a few MB of text; RichEdit's default typing limit is 32K. */
#define EDITOR_TEXT_LIMIT (16L * 1024 * 1024)

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

HWND editor_create(HWND parent)
{
    HWND editor;

    if (LoadLibrary("RICHED20.DLL") == NULL) {
        return NULL;
    }
    editor = CreateWindowEx(0, RICHEDIT_CLASSA, "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL
                                | ES_NOHIDESEL | ES_SAVESEL,
                            0, 0, 0, 0, parent, NULL, app.instance, NULL);
    if (editor == NULL) {
        return NULL;
    }
    SendMessage(editor, EM_EXLIMITTEXT, 0, EDITOR_TEXT_LIMIT);
    /* No automatic font switching: everything stays in the editor font. */
    SendMessage(editor, EM_SETLANGOPTIONS, 0, 0);
    SendMessage(editor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL | ENM_KEYEVENTS);
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
    PARAFORMAT2 paragraph;
    CHARRANGE selection;
    BOOL modified = (BOOL)SendMessage(app.editor, EM_GETMODIFY, 0, 0);

    SendMessage(app.editor, WM_SETREDRAW, FALSE, 0);
    SendMessage(app.editor, EM_SETBKGNDCOLOR, 0, (LPARAM)app.theme.background);
    base_format(&format);
    SendMessage(app.editor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
    SendMessage(app.editor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);

    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.cbSize = sizeof(paragraph);
    paragraph.dwMask = PFM_LINESPACING | PFM_SPACEAFTER;
    /* Rule 5: dyLineSpacing is in twentieths of a line; 30 = 1.5 lines. */
    paragraph.bLineSpacingRule = 5;
    paragraph.dyLineSpacing = 30;
    paragraph.dySpaceAfter = 0;
    SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&selection);
    SendMessage(app.editor, EM_HIDESELECTION, TRUE, 0);
    SendMessage(app.editor, EM_SETSEL, 0, -1);
    SendMessage(app.editor, EM_SETPARAFORMAT, 0, (LPARAM)&paragraph);
    SendMessage(app.editor, EM_EXSETSEL, 0, (LPARAM)&selection);
    SendMessage(app.editor, EM_HIDESELECTION, FALSE, 0);

    SendMessage(app.editor, EM_SETMODIFY, modified, 0);
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

    source.data = text;
    source.size = size;
    source.position = 0;
    stream.dwCookie = (DWORD_PTR)&source;
    stream.dwError = 0;
    stream.pfnCallback = read_stream;
    SendMessage(app.editor, WM_SETREDRAW, FALSE, 0);
    SendMessage(app.editor, EM_STREAMIN, SF_TEXT, (LPARAM)&stream);
    SendMessage(app.editor, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessage(app.editor, EM_SETSEL, 0, 0);
    SendMessage(app.editor, WM_SETREDRAW, TRUE, 0);
    editor_apply_settings();
    SendMessage(app.editor, EM_SETMODIFY, FALSE, 0);
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
    GETTEXTLENGTHEX length_query;
    TEXTRANGE range;
    LONG length;
    char *text;

    length_query.flags = GTL_NUMCHARS | GTL_PRECISE;
    length_query.codepage = CP_ACP;
    length = (LONG)SendMessage(app.editor, EM_GETTEXTLENGTHEX, (WPARAM)&length_query, 0);
    if (length < 0) {
        length = 0;
    }
    text = malloc((size_t)length + 1);
    if (text == NULL) {
        return NULL;
    }
    range.chrg.cpMin = 0;
    range.chrg.cpMax = length;
    range.lpstrText = text;
    length = (LONG)SendMessage(app.editor, EM_GETTEXTRANGE, 0, (LPARAM)&range);
    text[length] = '\0';
    *size = (size_t)length;
    return text;
}
