#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#include "app.h"
#include "editor.h"
#include "scanner.h"
#include "stats.h"
#include "style.h"
#include "tom.h"
#include "trace.h"

/* Color classes of a styled character. */
#define COLOR_TEXT 0
#define COLOR_MARKER 1
#define COLOR_FADED 2

/* Per-character format key: color class plus effects. */
#define KEY_BOLD 0x04
#define KEY_ITALIC 0x08
#define KEY_STRIKE 0x10
#define KEY_CODE 0x20

struct fence_entry {
    long offset;  /* paragraph start */
    struct fence_info info;
};

int style_busy;

static struct fence_entry *fences;
static size_t fence_count;
static size_t fence_capacity;

/* Paragraph range waiting to be restyled; -1 when empty. */
static long dirty_start = -1;
static long dirty_end = -1;
static int visible_dirty;
static int caret_dirty;

/* Focus region currently drawn, as character positions; -1 when none. It
 * can span several lines: a Markdown paragraph is often hard-wrapped. */
static long focus_start = -1;
static long focus_end = -1;

static long cached_first_line = -1;
static int padding_applied;

static unsigned char *styles;
static size_t styles_capacity;

/* 0: not tried yet, 1: TOM available, -1: no styling possible. */
static int tom_state;
/* One standalone TOM font per format key, built on first use. */
static struct tom_font *fonts[64];

struct batch {
    int repaint_all;
    LRESULT event_mask;
    BOOL modified;
    long top;
};

static long text_length(void)
{
    GETTEXTLENGTHEX query;

    query.flags = GTL_NUMCHARS;
    query.codepage = CP_ACP;
    return (long)SendMessage(app.editor, EM_GETTEXTLENGTHEX, (WPARAM)&query, 0);
}

static long get_range_into(long start, long end, char *text)
{
    TEXTRANGE range;

    range.chrg.cpMin = start;
    range.chrg.cpMax = end;
    range.lpstrText = text;
    return (long)SendMessage(app.editor, EM_GETTEXTRANGE, 0, (LPARAM)&range);
}

/* Returns the text in [start, end) (malloc'ed) and its length in *size. */
static char *get_range(long start, long end, long *size)
{
    char *text = malloc((size_t)(end - start) + 1);

    if (text == NULL) {
        return NULL;
    }
    *size = end > start ? get_range_into(start, end, text) : 0;
    text[*size] = '\0';
    return text;
}

#define SCAN_WINDOW 512

static long paragraph_start(long position)
{
    char window[SCAN_WINDOW + 1];

    while (position > 0) {
        long from = position > SCAN_WINDOW ? position - SCAN_WINDOW : 0;
        long count = get_range_into(from, position, window);
        long i;

        for (i = count; i > 0; i--) {
            if (window[i - 1] == '\r') {
                return from + i;
            }
        }
        position = from;
    }
    return 0;
}

/* Position of the '\r' ending the paragraph, or the text length. */
static long paragraph_end(long position, long length)
{
    char window[SCAN_WINDOW + 1];

    while (position < length) {
        long to = position + SCAN_WINDOW < length ? position + SCAN_WINDOW : length;
        long count = get_range_into(position, to, window);
        long i;

        for (i = 0; i < count; i++) {
            if (window[i] == '\r') {
                return position + i;
            }
        }
        if (count == 0) {
            break;
        }
        position += count;
    }
    return length;
}

/* ---- Fence index ---- */

static void fences_insert(long offset, const struct fence_info *info)
{
    size_t index = fence_count;

    if (fence_count == fence_capacity) {
        size_t capacity = fence_capacity == 0 ? 32 : fence_capacity * 2;
        struct fence_entry *grown = realloc(fences, capacity * sizeof(*grown));

        if (grown == NULL) {
            return;
        }
        fences = grown;
        fence_capacity = capacity;
    }
    while (index > 0 && fences[index - 1].offset > offset) {
        fences[index] = fences[index - 1];
        index--;
    }
    fences[index].offset = offset;
    fences[index].info = *info;
    fence_count++;
}

/* Removes entries with from <= offset <= to; returns how many. */
static size_t fences_remove(long from, long to)
{
    size_t kept = 0;
    size_t i;

    for (i = 0; i < fence_count; i++) {
        if (fences[i].offset < from || fences[i].offset > to) {
            fences[kept++] = fences[i];
        }
    }
    i = fence_count - kept;
    fence_count = kept;
    return i;
}

/* Adds the fence lines of text, which starts at paragraph start `from`;
 * returns how many. */
static size_t fences_scan(long from, const char *text, long size)
{
    size_t added = 0;
    long line = 0;

    while (line <= size) {
        long end = line;
        struct fence_info info;

        while (end < size && text[end] != '\r') {
            end++;
        }
        if (scan_fence_info(text + line, (size_t)(end - line), &info)) {
            fences_insert(from + line, &info);
            added++;
        }
        line = end + 1;
    }
    return added;
}

static void fences_rebuild(void)
{
    size_t size;
    char *text = editor_get_raw_text(&size);

    fence_count = 0;
    trace("  got text for fences");
    if (text != NULL) {
        fences_scan(0, text, (long)size);
        free(text);
    }
    trace("  fences rebuilt: %u", (unsigned)fence_count);
}

static void state_at(long offset, struct scan_state *state)
{
    size_t i;

    scan_state_init(state);
    for (i = 0; i < fence_count && fences[i].offset < offset; i++) {
        scan_state_apply_fence(state, &fences[i].info);
    }
}

/* ---- Applying formats ---- */

static void begin_batch(struct batch *batch, int repaint_all)
{
    style_busy++;
    batch->repaint_all = repaint_all;
    batch->event_mask = SendMessage(app.editor, EM_GETEVENTMASK, 0, 0);
    SendMessage(app.editor, EM_SETEVENTMASK, 0, 0);
    batch->modified = (BOOL)SendMessage(app.editor, EM_GETMODIFY, 0, 0);
    SendMessage(app.editor, WM_SETREDRAW, FALSE, 0);
    batch->top = -1;
}

/* Height of one text line in pixels: 1.5 times the font's line height
 * (see editor_apply_settings). */
static long line_height(void)
{
    HDC dc = GetDC(app.editor);
    HFONT font = CreateFont(-MulDiv(app.settings.font_size, GetDeviceCaps(dc, LOGPIXELSY), 72),
                            0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
                            app.settings.font_name);
    HGDIOBJ previous = SelectObject(dc, font);
    TEXTMETRIC metrics;
    long height;

    GetTextMetrics(dc, &metrics);
    SelectObject(dc, previous);
    DeleteObject(font);
    ReleaseDC(app.editor, dc);
    height = (metrics.tmHeight + metrics.tmExternalLeading) * 3 / 2;
    return height > 0 ? height : 24;
}

static void end_batch(struct batch *batch)
{
    long first_line = (long)SendMessage(app.editor, EM_GETFIRSTVISIBLELINE, 0, 0);
    RECT area;

    SendMessage(app.editor, WM_SETREDRAW, TRUE, 0);
    SendMessage(app.editor, EM_SETMODIFY, batch->modified, 0);
    SendMessage(app.editor, EM_SETEVENTMASK, 0, batch->event_mask);
    GetClientRect(app.editor, &area);
    /* Repaint from the first restyled line down. EM_POSFROMCHAR returns 0,0
     * on Windows 98's RichEdit here, so estimate from line numbers, with a
     * line height that is surely too small (the font alone) so the estimate
     * errs upwards. */
    if (batch->top >= 0 && !batch->repaint_all) {
        RECT column;
        HDC dc = GetDC(app.editor);
        long font_height = MulDiv(app.settings.font_size, GetDeviceCaps(dc, LOGPIXELSY), 72);
        long top_line = (long)SendMessage(app.editor, EM_EXLINEFROMCHAR, 0, batch->top);
        long top;

        ReleaseDC(app.editor, dc);
        SendMessage(app.editor, EM_GETRECT, 0, (LPARAM)&column);
        top = column.top + (top_line - first_line) * font_height - font_height;
        if (top > area.top) {
            area.top = top;
        }
    }
    if (area.bottom > area.top) {
        InvalidateRect(app.editor, &area, FALSE);
    }
    style_busy--;
}

static void free_fonts(void)
{
    size_t i;

    for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        tom_font_free(fonts[i]);
        fonts[i] = NULL;
    }
}

static void apply_format(long start, long end, unsigned key)
{
    if (fonts[key] == NULL) {
        unsigned color = key & 0x03;

        fonts[key] = tom_font_create(0, color == COLOR_MARKER ? app.theme.marker
                                        : color == COLOR_FADED ? app.theme.faded : app.theme.text,
                                     (key & KEY_BOLD) != 0, (key & KEY_ITALIC) != 0,
                                     (key & KEY_STRIKE) != 0,
                                     (key & KEY_CODE) ? (long)app.theme.code_background : TOM_AUTO_COLOR);
    }
    tom_apply_font(start, end, fonts[key]);
}

/* Half the view height in twips. */
static long typewriter_padding(void)
{
    RECT area;
    HDC dc = GetDC(app.editor);
    long pixels_per_inch = GetDeviceCaps(dc, LOGPIXELSY);

    ReleaseDC(app.editor, dc);
    GetClientRect(app.editor, &area);
    return (long)MulDiv(area.bottom / 2, 1440, (int)pixels_per_inch);
}

/* Line spacing (1.5 lines) and space after for one paragraph, in twips.
 * Typewriter mode gives the last paragraph half a view of space after it,
 * so the last line can scroll up to the middle. (No room above the first
 * line: RichEdit scrolls by whole lines, and a tall first line would make
 * scrolling jump.) */
static void set_paragraph_format(long paragraph, long space_after)
{
    tom_apply_paragraph(paragraph, (float)space_after / 20.0f);
}

static unsigned key_for(unsigned char span, int faded)
{
    unsigned key = faded ? COLOR_FADED : ((span & SPAN_MARKER) ? COLOR_MARKER : COLOR_TEXT);

    if (span & (SPAN_HEADING | SPAN_STRONG)) {
        key |= KEY_BOLD;
    }
    if (span & SPAN_EMPHASIS) {
        key |= KEY_ITALIC;
    }
    if (span & SPAN_STRIKE) {
        key |= KEY_STRIKE;
    }
    if (span & SPAN_CODE) {
        key |= KEY_CODE;
    }
    return key;
}

static int ensure_styles(size_t size)
{
    if (size > styles_capacity) {
        unsigned char *grown = realloc(styles, size);

        if (grown == NULL) {
            return 0;
        }
        styles = grown;
        styles_capacity = size;
    }
    return 1;
}

/* Styles one paragraph [start, start + size) whose text is `text`. */
static void style_paragraph(long start, const char *text, long size, long length,
                            struct scan_state *state, struct batch *batch)
{
    long end = start + size;
    long i = 0;
    /* In focus mode, characters outside [fade_from, fade_to) are faded. */
    long fade_from = 0;
    long fade_to = size;
    int base_faded;

    if (!ensure_styles((size_t)size + 1)) {
        return;
    }
    scan_line(text, (size_t)size, state, styles);

    if (app.settings.focus) {
        fade_from = focus_start - start;
        fade_to = focus_end - start;
        if (focus_start < 0 || fade_to <= 0 || fade_from >= size) {
            fade_from = 0;
            fade_to = 0;
        }
    }
    base_faded = app.settings.focus && fade_from >= fade_to;
    /* The base format covers the paragraph mark too, so text typed at the
     * end of the paragraph starts plain. */
    apply_format(start, end < length ? end + 1 : end, base_faded ? COLOR_FADED : COLOR_TEXT);
    while (i < size) {
        int faded = app.settings.focus && (i < fade_from || i >= fade_to);
        unsigned key = key_for(styles[i], faded);
        long run = i + 1;

        while (run < size
               && key_for(styles[run], app.settings.focus && (run < fade_from || run >= fade_to)) == key) {
            run++;
        }
        if (key != (base_faded ? COLOR_FADED : COLOR_TEXT)) {
            apply_format(start + i, start + run, key);
        }
        i = run;
    }
    set_paragraph_format(start, app.settings.typewriter && end >= length ? typewriter_padding() : 0);
    if (batch->top < 0 || start < batch->top) {
        batch->top = start;
    }
}

/* Restyles the paragraphs overlapping [from, to]. repaint_all repaints the
 * whole view instead of the restyled lines and below; use it after
 * scrolling, when line numbers of a large document may not be laid out. */
static void restyle(long from, long to, int repaint_all)
{
    struct batch batch;
    struct scan_state state;
    long length = text_length();
    long size;
    long line;
    char *text;

    if (from > length) {
        from = length;
    }
    if (to > length) {
        to = length;
    }
    from = paragraph_start(from);
    to = paragraph_end(to, length);
    text = get_range(from, to, &size);
    if (text == NULL) {
        return;
    }
    state_at(from, &state);
    trace("restyle %ld..%ld (length %ld, fences %u)", from, to, length, (unsigned)fence_count);
    begin_batch(&batch, repaint_all);
    line = 0;
    while (line <= size) {
        long end = line;

        while (end < size && text[end] != '\r') {
            end++;
        }
        style_paragraph(from + line, text + line, end - line, length, &state, &batch);
        line = end + 1;
    }
    end_batch(&batch);
    free(text);
}

static void visible_range(long *start, long *end)
{
    RECT area;
    POINTL corner;
    long first_line = (long)SendMessage(app.editor, EM_GETFIRSTVISIBLELINE, 0, 0);

    SendMessage(app.editor, EM_GETRECT, 0, (LPARAM)&area);
    *start = (long)SendMessage(app.editor, EM_LINEINDEX, (WPARAM)first_line, 0);
    corner.x = area.right - 1;
    corner.y = area.bottom - 1;
    *end = (long)SendMessage(app.editor, EM_CHARFROMPOS, 0, (LPARAM)&corner);
    if (*end < *start) {
        *end = *start;
    }
}

static void mark_dirty(long start, long end)
{
    if (dirty_start < 0 || start < dirty_start) {
        dirty_start = start;
    }
    if (end > dirty_end) {
        dirty_end = end;
    }
}

static long shift_position(long position, long start, long removed, long inserted)
{
    if (position <= start) {
        return position;
    }
    if (position < start + removed) {
        return start + inserted;
    }
    return position + inserted - removed;
}

/* ---- Focus and typewriter ---- */

/* The Markdown paragraph around position: the run of non-empty lines (a
 * paragraph is often hard-wrapped over several). An empty line gives an
 * empty range. Bounded, so a huge unbroken block stays cheap. */
static void markdown_paragraph(long position, long length, long *start, long *end)
{
    long first = paragraph_start(position);
    long last = paragraph_end(position, length);
    int lines;

    *start = first;
    *end = last;
    if (first == last) {
        return;
    }
    for (lines = 0; lines < 200 && *start > 0; lines++) {
        long previous = paragraph_start(*start - 1);

        if (previous == *start - 1) {
            break;
        }
        *start = previous;
    }
    for (lines = 0; lines < 200 && *end < length; lines++) {
        long next = paragraph_end(*end + 1, length);

        if (next == *end + 1) {
            break;
        }
        *end = next;
    }
}

static void update_focus(long caret)
{
    long length = text_length();
    long start;
    long end;
    long new_start;
    long new_end;

    markdown_paragraph(caret, length, &start, &end);
    new_start = start;
    new_end = end;
    if (start == end) {
        new_start = caret;
        new_end = caret;
    } else if (!app.settings.focus_paragraph) {
        long size;
        char *text = get_range(start, end, &size);
        size_t sentence_start;
        size_t sentence_end;

        if (text == NULL) {
            return;
        }
        sentence_bounds(text, (size_t)size, (size_t)(caret - start), &sentence_start, &sentence_end);
        new_start = start + (long)sentence_start;
        new_end = start + (long)sentence_end;
        free(text);
    }
    if (new_start == focus_start && new_end == focus_end) {
        return;
    }
    if (focus_start >= 0) {
        mark_dirty(focus_start, focus_end);
    }
    focus_start = new_start;
    focus_end = new_end;
    mark_dirty(new_start, new_end);
}

static void center_caret(void)
{
    CHARRANGE selection;
    RECT column;
    long height = line_height();
    long first_line = (long)SendMessage(app.editor, EM_GETFIRSTVISIBLELINE, 0, 0);
    long caret_line;
    long middle_line;

    SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&selection);
    SendMessage(app.editor, EM_GETRECT, 0, (LPARAM)&column);
    caret_line = (long)SendMessage(app.editor, EM_EXLINEFROMCHAR, 0, selection.cpMax);
    middle_line = first_line + (column.bottom - column.top) / height / 2;
    /* WM_VSCROLL line steps rather than EM_LINESCROLL, which jumped to the
     * top of a 1 MB document on Windows 98. */
    while (caret_line > middle_line) {
        SendMessage(app.editor, WM_VSCROLL, SB_LINEDOWN, 0);
        caret_line--;
    }
    while (caret_line < middle_line && first_line > 0) {
        SendMessage(app.editor, WM_VSCROLL, SB_LINEUP, 0);
        caret_line++;
        first_line--;
    }
}

static void open_tom(void)
{
    if (tom_state == 0 && app.editor != NULL) {
        tom_state = tom_open(app.editor) ? 1 : -1;
        trace("text object model %s", tom_state > 0 ? "available" : "missing: no styling");
    }
}

/* ---- Public ---- */

void style_reset(void)
{
    trace("reset");
    open_tom();
    free_fonts();
    fences_rebuild();
    dirty_start = -1;
    dirty_end = -1;
    focus_start = -1;
    focus_end = -1;
    visible_dirty = 1;
    caret_dirty = 1;
    cached_first_line = -1;
    if (app.settings.typewriter) {
        long length = text_length();
        struct batch batch;
        long padding = typewriter_padding();

        begin_batch(&batch, 1);
        set_paragraph_format(paragraph_start(length), padding);
        end_batch(&batch);
        padding_applied = 1;
    }
    style_idle();
}

void style_edit(long start, long removed, long inserted)
{
    trace("edit at %ld: -%ld +%ld", start, removed, inserted);
    long length = text_length();
    long check_start;
    long check_end;
    long size;
    char *text;
    size_t i;
    size_t changes;

    for (i = 0; i < fence_count; i++) {
        if (fences[i].offset > start) {
            fences[i].offset = shift_position(fences[i].offset, start, removed, inserted);
        }
    }
    if (dirty_start >= 0) {
        dirty_start = shift_position(dirty_start, start, removed, inserted);
        dirty_end = shift_position(dirty_end, start, removed, inserted);
    }
    if (focus_start >= 0) {
        focus_start = shift_position(focus_start, start, removed, inserted);
        focus_end = shift_position(focus_end, start, removed, inserted);
    }

    check_start = paragraph_start(start);
    check_end = paragraph_end(start + inserted, length);
    changes = fences_remove(check_start, check_end);
    text = get_range(check_start, check_end, &size);
    if (text != NULL) {
        changes += fences_scan(check_start, text, size);
        free(text);
    }
    /* A fence line appeared or went away: everything below may change. */
    if (changes > 0) {
        visible_dirty = 1;
    }
    mark_dirty(check_start, check_end);
    caret_dirty = 1;
}

void style_view_changed(void)
{
    caret_dirty = 1;
}

void style_settings_changed(void)
{
    trace("settings changed");
    free_fonts();
    if (!app.settings.typewriter && padding_applied) {
        long length = text_length();
        struct batch batch;

        begin_batch(&batch, 1);
        set_paragraph_format(paragraph_start(length), 0);
        end_batch(&batch);
        padding_applied = 0;
    } else if (app.settings.typewriter) {
        padding_applied = 1;
        mark_dirty(text_length(), text_length());
    }
    focus_start = -1;
    focus_end = -1;
    visible_dirty = 1;
    caret_dirty = 1;
}

int style_idle(void)
{
    int worked = 0;
    long first_line;
    int pass;

    open_tom();
    if (app.editor == NULL || app.previewing || tom_state <= 0) {
        return 0;
    }
    if (caret_dirty) {
        CHARRANGE selection;

        caret_dirty = 0;
        SendMessage(app.editor, EM_EXGETSEL, 0, (LPARAM)&selection);
        if (app.settings.focus) {
            update_focus(selection.cpMax);
        }
        if (app.settings.typewriter) {
            center_caret();
        }
        worked = 1;
    }
    if (dirty_start >= 0) {
        long start = dirty_start;
        long end = dirty_end;

        dirty_start = -1;
        dirty_end = -1;
        restyle(start, end, 0);
        worked = 1;
    }
    /* Restyling can't scroll (end_batch restores the view), but typewriter
     * centering can; two passes settle it. */
    for (pass = 0; pass < 2; pass++) {
        first_line = (long)SendMessage(app.editor, EM_GETFIRSTVISIBLELINE, 0, 0);
        if (first_line != cached_first_line) {
            visible_dirty = 1;
        }
        if (!visible_dirty) {
            break;
        }
        {
            long start;
            long end;

            visible_dirty = 0;
            visible_range(&start, &end);
            restyle(start, end, 1);
            cached_first_line = (long)SendMessage(app.editor, EM_GETFIRSTVISIBLELINE, 0, 0);
            worked = 1;
        }
    }
    return worked;
}
