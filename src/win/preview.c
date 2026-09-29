#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#include "app.h"
#include "buffer.h"
#include "editor.h"
#include "encoding.h"
#include "md4c.h"
#include "preview.h"
#include "rtf.h"
#include "style.h"

struct stream_source {
    const char *data;
    size_t size;
    size_t position;
};

/* The Markdown that was rendered, and where its text landed. */
static struct rtf_anchors anchors;
static char *source;
static size_t source_size;
static CHARRANGE editor_selection;
static long shown_caret;
static long shown_first_line;

static DWORD CALLBACK read_stream(DWORD_PTR cookie, LPBYTE destination, LONG wanted, LONG *copied)
{
    struct stream_source *stream = (struct stream_source *)cookie;
    size_t left = stream->size - stream->position;
    size_t count = (size_t)wanted < left ? (size_t)wanted : left;

    memcpy(destination, stream->data + stream->position, count);
    stream->position += count;
    *copied = (LONG)count;
    return 0;
}

HWND preview_create(HWND parent)
{
    HWND preview = CreateWindowEx(0, RICHEDIT_CLASSA, "",
                                  WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL
                                      | ES_NOHIDESEL,
                                  0, 0, 0, 0, parent, NULL, app.instance, NULL);

    if (preview != NULL) {
        SendMessage(preview, EM_EXLIMITTEXT, 0, 16L * 1024 * 1024);
        SendMessage(preview, EM_SETLANGOPTIONS, 0, 0);
    }
    return preview;
}

void preview_apply_settings(void)
{
    if (app.preview != NULL) {
        SendMessage(app.preview, EM_SETBKGNDCOLOR, 0, (LPARAM)app.theme.background);
        editor_layout_control(app.preview);
        InvalidateRect(app.preview, NULL, TRUE);
    }
}

static void set_caret(HWND control, long position)
{
    CHARRANGE range;

    range.cpMin = position;
    range.cpMax = position;
    SendMessage(control, EM_EXSETSEL, 0, (LPARAM)&range);
    SendMessage(control, EM_SCROLLCARET, 0, 0);
}

void preview_show(void)
{
    struct rtf_theme theme;
    struct buffer rtf;
    struct stream_source stream_source;
    EDITSTREAM stream;
    size_t text_size;
    char *text = editor_get_raw_text(&text_size);
    long rendered;

    if (text == NULL || app.preview == NULL) {
        free(text);
        return;
    }
    free(source);
    source = encoding_to_utf8(text, text_size, ENCODING_CP1252, &source_size);
    if (source == NULL) {
        free(text);
        return;
    }
    theme.font_name = app.settings.font_name;
    theme.font_size = app.settings.font_size;
    theme.text_color = app.theme.text;
    theme.muted_color = app.theme.marker;
    theme.code_background = app.theme.code_background;
    theme.link_color = app.theme.link;
    buffer_init(&rtf);
    if (rtf_render(source, source_size, MD_DIALECT_GITHUB, &theme, &rtf, &anchors) != 0) {
        buffer_free(&rtf);
        free(text);
        app_error("Can't render the preview%s.", "");
        return;
    }
    stream_source.data = rtf.data;
    stream_source.size = rtf.size;
    stream_source.position = 0;
    stream.dwCookie = (DWORD_PTR)&stream_source;
    stream.dwError = 0;
    stream.pfnCallback = read_stream;
    SendMessage(app.preview, WM_SETREDRAW, FALSE, 0);
    SendMessage(app.preview, EM_STREAMIN, SF_RTF, (LPARAM)&stream);
    SendMessage(app.preview, WM_SETREDRAW, TRUE, 0);
    buffer_free(&rtf);
    preview_apply_settings();

    SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&editor_selection);
    rendered = (long)rtf_source_to_rendered(&anchors,
                                             cp1252_utf8_offset(text, (size_t)editor_selection.cpMin));
    free(text);
    app.previewing = 1;
    ShowWindow(app.editor, SW_HIDE);
    ShowWindow(app.preview, SW_SHOW);
    SetFocus(app.preview);
    set_caret(app.preview, rendered);
    shown_caret = rendered;
    shown_first_line = (long)SendMessage(app.preview, EM_GETFIRSTVISIBLELINE, 0, 0);
}

void preview_hide(void)
{
    CHARRANGE selection;
    long first_line;
    long target = -1;

    if (!app.previewing) {
        return;
    }
    SendMessage(app.preview, EM_EXGETSEL, 0, (LPARAM)&selection);
    first_line = (long)SendMessage(app.preview, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (selection.cpMin != shown_caret) {
        target = selection.cpMin;
    } else if (first_line != shown_first_line) {
        target = (long)SendMessage(app.preview, EM_LINEINDEX, (WPARAM)first_line, 0);
    }
    app.previewing = 0;
    ShowWindow(app.preview, SW_HIDE);
    ShowWindow(app.editor, SW_SHOW);
    SetFocus(app.editor);
    if (target >= 0 && source != NULL) {
        size_t byte = rtf_rendered_to_source(&anchors, (size_t)target);

        set_caret(app.editor, (long)utf8_cp1252_offset(source, source_size, byte));
    } else {
        SendMessage(app.editor, EM_EXSETSEL, 0, (LPARAM)&editor_selection);
        SendMessage(app.editor, EM_SCROLLCARET, 0, 0);
    }
    style_view_changed();
}
