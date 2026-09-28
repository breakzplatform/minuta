#include "rtf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoding.h"
#include "entity.h"
#include "md4c.h"

/* Color table indexes (see write_header). */
#define COLOR_TEXT 1
#define COLOR_MUTED 2
#define COLOR_CODE 3
#define COLOR_LINK 4

#define MAX_LIST_DEPTH 32
#define RULE_LENGTH 24
#define NO_SOURCE ((size_t)-1)
#define REPLACEMENT_CHARACTER 0xFFFDUL

enum paragraph_kind {
    KIND_TEXT,
    KIND_HEADING,
    KIND_CODE,
    KIND_HTML,
    KIND_RULE,
    KIND_TABLE_ROW
};

struct list_level {
    int ordered;
    int tight;
    unsigned next_number;
};

struct table_cell {
    size_t start;   /* into table.text */
    size_t length;
    size_t source;  /* offset of the cell's first text in the Markdown, or NO_SOURCE */
    unsigned row;
    MD_ALIGN align;
};

/* Tables are collected as plain text and written once complete, because
 * column widths depend on every row. */
struct table {
    struct buffer text;
    struct table_cell *cells;
    size_t cell_count;
    size_t cell_capacity;
    unsigned row_count;
    int in_cell;
};

struct renderer {
    const char *markdown;
    size_t size;
    const struct rtf_theme *theme;
    struct buffer *out;
    struct rtf_anchors *anchors;
    size_t rendered;        /* characters RichEdit holds so far */
    int failed;
    long body_size;         /* half-points */
    long indent_step;       /* twips */
    long space_after;       /* twips */
    enum paragraph_kind kind;
    unsigned heading_level;
    int paragraph_open;
    size_t paragraph_end;   /* out->size right after the latest \par */
    int pending_line;       /* a verbatim block ended a line; \line once more follows */
    int quote_depth;
    int list_depth;
    struct list_level lists[MAX_LIST_DEPTH];
    int marker_pending;
    int marker_ordered;
    unsigned marker_number;
    char marker_task;       /* 0 when the item is not a task */
    int image_depth;
    int in_table;
    int last_table_row;
    struct table table;
};

static void append(struct renderer *r, const char *text)
{
    buffer_append_string(r->out, text);
}

static void append_number(struct renderer *r, const char *word, long value)
{
    char text[48];

    sprintf(text, "%s%ld", word, value);
    append(r, text);
}

static void add_anchor(struct renderer *r, size_t source)
{
    struct rtf_anchors *anchors = r->anchors;
    struct rtf_anchor *items;
    size_t capacity;

    if (anchors == NULL || source == NO_SOURCE) {
        return;
    }
    if (anchors->count > 0) {
        const struct rtf_anchor *last = &anchors->items[anchors->count - 1];

        if (source <= last->source || r->rendered < last->rendered) {
            return;
        }
    }
    if (anchors->count == anchors->capacity) {
        capacity = anchors->capacity < 64 ? 64 : anchors->capacity * 2;
        items = realloc(anchors->items, capacity * sizeof(*items));
        if (items == NULL) {
            r->failed = 1;
            return;
        }
        anchors->items = items;
        anchors->capacity = capacity;
    }
    anchors->items[anchors->count].source = source;
    anchors->items[anchors->count].rendered = r->rendered;
    anchors->count++;
}

static void append_utf16_unit(struct renderer *r, unsigned long unit)
{
    long value = (long)unit;

    if (value > 32767) {
        value -= 65536;
    }
    append_number(r, "\\u", value);
    append(r, "?");
    r->rendered++;
}

static void emit_code_point(struct renderer *r, unsigned long code_point)
{
    char text[8];
    int byte;

    if (code_point == '\\' || code_point == '{' || code_point == '}') {
        text[0] = '\\';
        text[1] = (char)code_point;
        buffer_append(r->out, text, 2);
    } else if (code_point >= 0x20 && code_point < 0x7F) {
        buffer_append_char(r->out, (char)code_point);
    } else if (code_point == '\t') {
        append(r, "\\tab ");
    } else if (code_point < 0x20 || code_point == 0x7F) {
        return;  /* other control characters have no visible form */
    } else {
        byte = codepage_from_unicode(ENCODING_CP1252, code_point);
        if (byte >= 0x80) {
            sprintf(text, "\\'%02x", (unsigned)byte);
            append(r, text);
        } else if (code_point > 0xFFFF) {
            code_point -= 0x10000;
            append_utf16_unit(r, 0xD800 + (code_point >> 10));
            append_utf16_unit(r, 0xDC00 + (code_point & 0x3FF));
            return;
        } else {
            append_utf16_unit(r, code_point);
            return;
        }
    }
    r->rendered++;
}

static void emit_ascii(struct renderer *r, const char *text)
{
    while (*text != '\0') {
        emit_code_point(r, (unsigned char)*text);
        text++;
    }
}

static void emit_line_break(struct renderer *r)
{
    append(r, "\\line ");
    r->rendered++;
}

static void flush_pending_line(struct renderer *r)
{
    if (r->pending_line) {
        r->pending_line = 0;
        emit_line_break(r);
    }
}

/* Emits UTF-8 text. When source is known, anchors are added at the start
 * and wherever the byte count stops matching the character count, so that
 * offsets inside an anchor can be mapped one to one. In verbatim blocks a
 * newline becomes \line, deferred so the block's final newline is dropped. */
static void emit_utf8(struct renderer *r, const char *text, size_t size, size_t source,
                      int verbatim)
{
    size_t i = 0;
    size_t length;
    size_t before;
    unsigned long code_point;
    unsigned char c;

    if (verbatim && size > 0) {
        flush_pending_line(r);
    }
    add_anchor(r, source);
    while (i < size) {
        before = r->rendered;
        c = (unsigned char)text[i];
        if (c == '\r') {
            length = 1;
        } else if (c == '\n') {
            length = 1;
            if (verbatim) {
                r->pending_line = 1;
            } else {
                emit_code_point(r, ' ');
            }
        } else {
            length = utf8_decode((const unsigned char *)text + i, size - i, &code_point);
            if (verbatim) {
                flush_pending_line(r);
            }
            emit_code_point(r, code_point);
        }
        i += length;
        if (source != NO_SOURCE && r->rendered - before != length) {
            add_anchor(r, source + i);
        }
    }
}

static unsigned long sanitize_code_point(unsigned long code_point)
{
    if (code_point == 0 || code_point > 0x10FFFF
        || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        return REPLACEMENT_CHARACTER;
    }
    return code_point;
}

/* Decodes "&name;", "&#NN;" or "&#xNN;". Returns the number of code points
 * (1 or 2), or 0 for an unknown name. */
static int decode_entity(const char *text, size_t size, unsigned long code_points[2])
{
    const ENTITY *entity;
    unsigned long value = 0;
    size_t i;
    int digit;
    int hex;

    if (size >= 4 && text[1] == '#') {
        hex = text[2] == 'x' || text[2] == 'X';
        for (i = hex ? 3 : 2; i < size && text[i] != ';'; i++) {
            digit = -1;
            if (text[i] >= '0' && text[i] <= '9') {
                digit = text[i] - '0';
            } else if (hex && text[i] >= 'a' && text[i] <= 'f') {
                digit = text[i] - 'a' + 10;
            } else if (hex && text[i] >= 'A' && text[i] <= 'F') {
                digit = text[i] - 'A' + 10;
            }
            if (digit < 0) {
                return 0;
            }
            if (value <= 0x10FFFF) {
                value = value * (hex ? 16UL : 10UL) + (unsigned long)digit;
            }
        }
        code_points[0] = sanitize_code_point(value);
        code_points[1] = 0;
        return 1;
    }
    entity = entity_lookup(text, size);
    if (entity == NULL) {
        return 0;
    }
    code_points[0] = entity->codepoints[0];
    code_points[1] = entity->codepoints[1];
    return code_points[1] != 0 ? 2 : 1;
}

static void emit_entity(struct renderer *r, const char *text, size_t size, size_t source)
{
    unsigned long code_points[2];
    int count;
    int i;

    count = decode_entity(text, size, code_points);
    if (count == 0) {
        emit_utf8(r, text, size, source, 0);
        return;
    }
    add_anchor(r, source);
    for (i = 0; i < count; i++) {
        emit_code_point(r, code_points[i]);
    }
}

static int list_index(const struct renderer *r)
{
    return (r->list_depth < MAX_LIST_DEPTH ? r->list_depth : MAX_LIST_DEPTH) - 1;
}

static void emit_marker(struct renderer *r)
{
    char number[16];

    if (r->marker_ordered) {
        sprintf(number, "%u.", r->marker_number);
        emit_ascii(r, number);
    } else {
        emit_code_point(r, 0x2022);
    }
    append(r, "\\tab ");
    r->rendered++;
    if (r->marker_task != 0) {
        emit_ascii(r, r->marker_task == ' ' ? "[ ] " : "[x] ");
    }
    r->marker_pending = 0;
}

/* Opens a paragraph (\pard plus a group for its character formatting),
 * shaped by the current block kind and the enclosing quotes and lists. */
static void begin_paragraph(struct renderer *r)
{
    long left;
    long before = 0;
    long after = r->space_after;
    long line = 300;
    int levels;
    size_t formatted;

    if (r->paragraph_open) {
        return;
    }
    levels = r->list_depth < MAX_LIST_DEPTH ? r->list_depth : MAX_LIST_DEPTH;
    left = r->indent_step * (long)(r->quote_depth + levels);
    if (levels > 0 && r->lists[levels - 1].tight) {
        after = r->space_after / 3;
    }
    switch (r->kind) {
    case KIND_HEADING:
        before = r->space_after;
        after = r->space_after / 2;
        break;
    case KIND_CODE:
        left += r->indent_step / 2;
        line = 240;
        break;
    case KIND_HTML:
        line = 240;
        break;
    case KIND_TABLE_ROW:
        line = 240;
        if (!r->last_table_row) {
            after = 0;
        }
        break;
    default:
        break;
    }
    append_number(r, "\\pard\\sl", line);
    append(r, "\\slmult1");
    append_number(r, "\\sb", before);
    append_number(r, "\\sa", after);
    append_number(r, "\\li", left);
    if (r->marker_pending) {
        append_number(r, "\\fi-", r->indent_step);
        append_number(r, "\\tx", left);
    }
    if (r->kind == KIND_RULE) {
        append(r, "\\qc");
    }
    append(r, " ");
    if (r->marker_pending) {
        emit_marker(r);
    }
    append(r, "{");
    formatted = r->out->size;
    switch (r->kind) {
    case KIND_HEADING:
        append(r, "\\b");
        if (r->heading_level == 1) {
            append_number(r, "\\fs", r->body_size * 3 / 2);
        } else if (r->heading_level == 2) {
            append_number(r, "\\fs", r->body_size * 13 / 10);
        } else if (r->heading_level == 3) {
            append_number(r, "\\fs", r->body_size * 115 / 100);
        }
        break;
    case KIND_CODE:
        append_number(r, "\\highlight", COLOR_CODE);
        break;
    case KIND_HTML:
    case KIND_RULE:
        append_number(r, "\\cf", COLOR_MUTED);
        break;
    default:
        break;
    }
    if (r->quote_depth > 0 && r->kind != KIND_CODE) {
        append_number(r, "\\cf", COLOR_MUTED);
    }
    if (r->out->size != formatted) {
        append(r, " ");
    }
    r->paragraph_open = 1;
}

static void end_paragraph(struct renderer *r)
{
    if (!r->paragraph_open) {
        return;
    }
    r->pending_line = 0;
    append(r, "}\\par\n");
    r->rendered++;
    r->paragraph_open = 0;
    r->paragraph_end = r->out->size;
}

/* A list item whose first child is not a paragraph still gets its marker,
 * on a line of its own. */
static void flush_marker(struct renderer *r)
{
    enum paragraph_kind kind = r->kind;

    if (!r->marker_pending) {
        return;
    }
    r->kind = KIND_TEXT;
    begin_paragraph(r);
    end_paragraph(r);
    r->kind = kind;
}

static void emit_rule(struct renderer *r)
{
    int i;

    r->kind = KIND_RULE;
    begin_paragraph(r);
    for (i = 0; i < RULE_LENGTH; i++) {
        emit_code_point(r, 0x2014);
    }
    end_paragraph(r);
    r->kind = KIND_TEXT;
}

static size_t count_code_points(const char *text, size_t size)
{
    size_t count = 0;
    size_t i = 0;
    unsigned long code_point;

    while (i < size) {
        i += utf8_decode((const unsigned char *)text + i, size - i, &code_point);
        count++;
    }
    return count;
}

static void emit_spaces(struct renderer *r, size_t count)
{
    while (count > 0) {
        emit_code_point(r, ' ');
        count--;
    }
}

static void table_reset(struct table *table)
{
    buffer_free(&table->text);
    free(table->cells);
    table->cells = NULL;
    table->cell_count = 0;
    table->cell_capacity = 0;
    table->row_count = 0;
    table->in_cell = 0;
}

static void table_begin_cell(struct renderer *r, const MD_BLOCK_TD_DETAIL *detail)
{
    struct table *table = &r->table;
    struct table_cell *cells;
    struct table_cell *cell;
    size_t capacity;

    if (table->cell_count == table->cell_capacity) {
        capacity = table->cell_capacity < 16 ? 16 : table->cell_capacity * 2;
        cells = realloc(table->cells, capacity * sizeof(*cells));
        if (cells == NULL) {
            r->failed = 1;
            return;
        }
        table->cells = cells;
        table->cell_capacity = capacity;
    }
    cell = &table->cells[table->cell_count++];
    cell->start = table->text.size;
    cell->length = 0;
    cell->source = NO_SOURCE;
    cell->row = table->row_count > 0 ? table->row_count - 1 : 0;
    cell->align = detail != NULL ? detail->align : MD_ALIGN_DEFAULT;
    table->in_cell = 1;
}

static void table_end_cell(struct renderer *r)
{
    struct table *table = &r->table;
    struct table_cell *cell;

    if (!table->in_cell || table->cell_count == 0) {
        return;
    }
    cell = &table->cells[table->cell_count - 1];
    cell->length = table->text.size - cell->start;
    table->in_cell = 0;
}

static void table_append_code_point(struct table *table, unsigned long code_point)
{
    char text[4];
    size_t length;

    length = utf8_encode(code_point, text);
    buffer_append(&table->text, text, length);
}

static void table_text(struct renderer *r, MD_TEXTTYPE type, const char *text, size_t size,
                       size_t source)
{
    struct table *table = &r->table;
    struct table_cell *cell;
    unsigned long code_points[2];
    int count;
    int i;

    if (!table->in_cell || table->cell_count == 0) {
        return;
    }
    cell = &table->cells[table->cell_count - 1];
    if (cell->source == NO_SOURCE) {
        cell->source = source;
    }
    switch (type) {
    case MD_TEXT_NULLCHAR:
        table_append_code_point(table, REPLACEMENT_CHARACTER);
        break;
    case MD_TEXT_BR:
    case MD_TEXT_SOFTBR:
        buffer_append_char(&table->text, ' ');
        break;
    case MD_TEXT_ENTITY:
        count = decode_entity(text, size, code_points);
        if (count == 0) {
            buffer_append(&table->text, text, size);
        }
        for (i = 0; i < count; i++) {
            table_append_code_point(table, code_points[i]);
        }
        break;
    default:
        buffer_append(&table->text, text, size);
        break;
    }
}

static void emit_table_cell(struct renderer *r, const struct table_cell *cell, size_t width,
                            MD_ALIGN align, int last_column)
{
    const char *text = "";
    size_t length = 0;
    size_t used;
    size_t padding;
    size_t left = 0;

    if (cell != NULL) {
        text = r->table.text.data + cell->start;
        length = cell->length;
    }
    used = count_code_points(text, length);
    padding = width > used ? width - used : 0;
    if (align == MD_ALIGN_RIGHT) {
        left = padding;
    } else if (align == MD_ALIGN_CENTER) {
        left = padding / 2;
    }
    emit_spaces(r, left);
    if (cell != NULL) {
        add_anchor(r, cell->source);
    }
    emit_utf8(r, text, length, NO_SOURCE, 0);
    if (!last_column) {
        emit_spaces(r, padding - left);
    }
}

/* Writes the collected table as monospace text rows: columns padded to a
 * common width and separated by " | ", the header in bold and underlined
 * by a row of dashes. */
static void emit_table(struct renderer *r)
{
    struct table *table = &r->table;
    size_t *widths;
    MD_ALIGN *aligns;
    const struct table_cell **row_cells;
    size_t columns = 0;
    size_t column;
    size_t index;
    size_t first;
    size_t width;
    unsigned row;

    if (table->cell_count == 0 || table->text.failed) {
        r->failed = r->failed || table->text.failed;
        return;
    }
    for (index = 0, first = 0; index <= table->cell_count; index++) {
        if (index == table->cell_count || table->cells[index].row != table->cells[first].row) {
            if (index - first > columns) {
                columns = index - first;
            }
            first = index;
        }
    }
    widths = calloc(columns, sizeof(*widths));
    aligns = calloc(columns, sizeof(*aligns));
    row_cells = calloc(columns, sizeof(*row_cells));
    if (widths == NULL || aligns == NULL || row_cells == NULL) {
        r->failed = 1;
        free(widths);
        free(aligns);
        free((void *)row_cells);
        return;
    }
    for (index = 0, first = 0; index < table->cell_count; index++) {
        if (table->cells[index].row != table->cells[first].row) {
            first = index;
        }
        column = index - first;
        width = count_code_points(table->text.data + table->cells[index].start,
                                  table->cells[index].length);
        if (width > widths[column]) {
            widths[column] = width;
        }
        if (table->cells[index].row == table->cells[0].row) {
            aligns[column] = table->cells[index].align;
        }
    }

    index = 0;
    for (row = table->cells[0].row; index < table->cell_count; row++) {
        for (column = 0; column < columns; column++) {
            row_cells[column] = NULL;
            if (index < table->cell_count && table->cells[index].row == row) {
                row_cells[column] = &table->cells[index++];
            }
        }
        while (index < table->cell_count && table->cells[index].row == row) {
            index++;
        }
        r->kind = KIND_TABLE_ROW;
        r->last_table_row = index == table->cell_count;
        begin_paragraph(r);
        if (row == table->cells[0].row) {
            append(r, "{\\b ");
        }
        for (column = 0; column < columns; column++) {
            if (column > 0) {
                emit_ascii(r, " | ");
            }
            emit_table_cell(r, row_cells[column], widths[column], aligns[column],
                            column + 1 == columns);
        }
        if (row == table->cells[0].row) {
            append(r, "}");
        }
        end_paragraph(r);
        if (row == table->cells[0].row) {
            begin_paragraph(r);
            for (column = 0; column < columns; column++) {
                if (column > 0) {
                    emit_ascii(r, "-|-");
                }
                for (width = 0; width < widths[column]; width++) {
                    emit_code_point(r, '-');
                }
            }
            end_paragraph(r);
        }
    }
    r->kind = KIND_TEXT;
    r->last_table_row = 0;
    free(widths);
    free(aligns);
    free((void *)row_cells);
}

static size_t source_offset(const struct renderer *r, const char *text)
{
    if (text >= r->markdown && text < r->markdown + r->size) {
        return (size_t)(text - r->markdown);
    }
    return NO_SOURCE;
}

static int status(const struct renderer *r)
{
    return (r->failed || r->out->failed) ? -1 : 0;
}

static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    struct renderer *r = userdata;
    struct list_level *level;
    const MD_BLOCK_LI_DETAIL *item;

    if (r->in_table) {
        if (type == MD_BLOCK_TR) {
            r->table.row_count++;
        } else if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) {
            table_begin_cell(r, detail);
        }
        return status(r);
    }
    switch (type) {
    case MD_BLOCK_QUOTE:
    case MD_BLOCK_ADMONITION:
        end_paragraph(r);
        flush_marker(r);
        r->quote_depth++;
        break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        end_paragraph(r);
        flush_marker(r);
        r->list_depth++;
        if (r->list_depth <= MAX_LIST_DEPTH) {
            level = &r->lists[r->list_depth - 1];
            level->ordered = type == MD_BLOCK_OL;
            if (level->ordered) {
                level->tight = ((MD_BLOCK_OL_DETAIL *)detail)->is_tight;
                level->next_number = ((MD_BLOCK_OL_DETAIL *)detail)->start;
            } else {
                level->tight = ((MD_BLOCK_UL_DETAIL *)detail)->is_tight;
                level->next_number = 0;
            }
        }
        break;
    case MD_BLOCK_LI:
        end_paragraph(r);
        flush_marker(r);
        if (r->list_depth > 0) {
            level = &r->lists[list_index(r)];
            item = detail;
            r->marker_pending = 1;
            r->marker_ordered = level->ordered;
            r->marker_number = level->next_number++;
            r->marker_task = 0;
            if (item != NULL && item->is_task) {
                r->marker_task = item->task_mark == ' ' ? ' ' : 'x';
            }
        }
        break;
    case MD_BLOCK_HR:
        end_paragraph(r);
        flush_marker(r);
        emit_rule(r);
        break;
    case MD_BLOCK_H:
        end_paragraph(r);
        r->kind = KIND_HEADING;
        r->heading_level = ((MD_BLOCK_H_DETAIL *)detail)->level;
        begin_paragraph(r);
        break;
    case MD_BLOCK_CODE:
        end_paragraph(r);
        r->kind = KIND_CODE;
        begin_paragraph(r);
        break;
    case MD_BLOCK_HTML:
        end_paragraph(r);
        r->kind = KIND_HTML;
        begin_paragraph(r);
        break;
    case MD_BLOCK_P:
        end_paragraph(r);
        r->kind = KIND_TEXT;
        begin_paragraph(r);
        break;
    case MD_BLOCK_TABLE:
        end_paragraph(r);
        flush_marker(r);
        table_reset(&r->table);
        r->in_table = 1;
        break;
    default:
        break;
    }
    return status(r);
}

static int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    struct renderer *r = userdata;

    (void)detail;
    if (r->in_table) {
        if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) {
            table_end_cell(r);
        } else if (type == MD_BLOCK_TABLE) {
            r->in_table = 0;
            emit_table(r);
            table_reset(&r->table);
        }
        return status(r);
    }
    switch (type) {
    case MD_BLOCK_QUOTE:
    case MD_BLOCK_ADMONITION:
        end_paragraph(r);
        flush_marker(r);
        r->quote_depth--;
        break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        end_paragraph(r);
        r->list_depth--;
        break;
    case MD_BLOCK_LI:
        end_paragraph(r);
        flush_marker(r);
        break;
    case MD_BLOCK_H:
    case MD_BLOCK_CODE:
    case MD_BLOCK_HTML:
    case MD_BLOCK_P:
        end_paragraph(r);
        r->kind = KIND_TEXT;
        break;
    default:
        break;
    }
    return status(r);
}

static int enter_span(MD_SPANTYPE type, void *detail, void *userdata)
{
    struct renderer *r = userdata;
    char label[24];

    if (r->in_table) {
        if (type == MD_SPAN_IMG) {
            buffer_append_string(&r->table.text, "[image: ");
        }
        return status(r);
    }
    begin_paragraph(r);
    switch (type) {
    case MD_SPAN_EM:
        append(r, "{\\i ");
        break;
    case MD_SPAN_STRONG:
        append(r, "{\\b ");
        break;
    case MD_SPAN_A:
        append_number(r, "{\\ul\\cf", COLOR_LINK);
        append(r, " ");
        break;
    case MD_SPAN_IMG:
        if (r->image_depth++ == 0) {
            append_number(r, "{\\cf", COLOR_MUTED);
            append(r, " ");
            emit_ascii(r, "[image: ");
        } else {
            append(r, "{");
        }
        break;
    case MD_SPAN_CODE:
    case MD_SPAN_MARK:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        append_number(r, "{\\highlight", COLOR_CODE);
        append(r, " ");
        break;
    case MD_SPAN_DEL:
        append(r, "{\\strike ");
        break;
    case MD_SPAN_U:
    case MD_SPAN_INS:
        append(r, "{\\ul ");
        break;
    case MD_SPAN_SUPERSCRIPT:
        append(r, "{\\super ");
        break;
    case MD_SPAN_SUBSCRIPT:
        append(r, "{\\sub ");
        break;
    case MD_SPAN_FOOTNOTE_REF:
        append(r, "{\\super ");
        sprintf(label, "[%u]", ((MD_SPAN_FOOTNOTE_REF_DETAIL *)detail)->id);
        emit_ascii(r, label);
        break;
    default:
        append(r, "{");
        break;
    }
    return status(r);
}

static int leave_span(MD_SPANTYPE type, void *detail, void *userdata)
{
    struct renderer *r = userdata;

    (void)detail;
    if (r->in_table) {
        if (type == MD_SPAN_IMG) {
            buffer_append_char(&r->table.text, ']');
        }
        return status(r);
    }
    if (type == MD_SPAN_IMG && --r->image_depth == 0) {
        emit_code_point(r, ']');
    }
    append(r, "}");
    return status(r);
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    struct renderer *r = userdata;
    size_t source = source_offset(r, text);
    int verbatim = r->kind == KIND_CODE || r->kind == KIND_HTML;

    if (r->in_table) {
        table_text(r, type, text, size, source);
        return status(r);
    }
    begin_paragraph(r);
    switch (type) {
    case MD_TEXT_NULLCHAR:
        add_anchor(r, source);
        emit_code_point(r, REPLACEMENT_CHARACTER);
        break;
    case MD_TEXT_BR:
        emit_line_break(r);
        break;
    case MD_TEXT_SOFTBR:
        emit_code_point(r, ' ');
        break;
    case MD_TEXT_ENTITY:
        emit_entity(r, text, size, source);
        break;
    case MD_TEXT_HTML:
        if (verbatim) {
            emit_utf8(r, text, size, source, 1);
        } else {
            append_number(r, "{\\cf", COLOR_MUTED);
            append(r, " ");
            emit_utf8(r, text, size, source, 0);
            append(r, "}");
        }
        break;
    default:
        emit_utf8(r, text, size, source, verbatim);
        break;
    }
    return status(r);
}

static void append_color(struct renderer *r, unsigned long color)
{
    append_number(r, "\\red", (long)(color & 0xFF));
    append_number(r, "\\green", (long)((color >> 8) & 0xFF));
    append_number(r, "\\blue", (long)((color >> 16) & 0xFF));
    append(r, ";");
}

static void write_header(struct renderer *r)
{
    const char *name = r->theme->font_name;
    size_t rendered = r->rendered;

    append(r, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern\\fcharset0 ");
    emit_utf8(r, name, strlen(name), NO_SOURCE, 0);
    append(r, ";}}\n{\\colortbl;");
    append_color(r, r->theme->text_color);
    append_color(r, r->theme->muted_color);
    append_color(r, r->theme->code_background);
    append_color(r, r->theme->link_color);
    append(r, "}\n\\uc1");
    /* A tab stop every four characters (a Courier-like cell is 0.6 em). */
    append_number(r, "\\deftab", (long)r->theme->font_size * 48);
    append_number(r, "\\f0\\fs", r->body_size);
    append_number(r, "\\cf", COLOR_TEXT);
    append(r, "\n");
    r->rendered = rendered;
}

int rtf_render(const char *markdown, size_t size, unsigned parser_flags,
               const struct rtf_theme *theme, struct buffer *out,
               struct rtf_anchors *anchors)
{
    struct renderer renderer;
    MD_PARSER parser;
    int result;
    int font_size;

    memset(&renderer, 0, sizeof(renderer));
    renderer.markdown = markdown;
    renderer.size = size;
    renderer.theme = theme;
    renderer.out = out;
    renderer.anchors = anchors;
    renderer.kind = KIND_TEXT;
    buffer_init(&renderer.table.text);
    font_size = theme->font_size > 0 ? theme->font_size : 10;
    renderer.body_size = (long)font_size * 2;
    renderer.indent_step = (long)font_size * 40;
    renderer.space_after = (long)font_size * 12;
    if (anchors != NULL) {
        anchors->count = 0;
    }

    memset(&parser, 0, sizeof(parser));
    parser.flags = parser_flags;
    parser.enter_block = enter_block;
    parser.leave_block = leave_block;
    parser.enter_span = enter_span;
    parser.leave_span = leave_span;
    parser.text = text_callback;

    write_header(&renderer);
    result = md_parse(markdown, (MD_SIZE)size, &parser, &renderer);
    end_paragraph(&renderer);
    table_reset(&renderer.table);
    if (result != 0 || renderer.failed || out->failed) {
        return -1;
    }
    /* Drop the last \par: RichEdit already ends with an empty paragraph. */
    if (renderer.rendered > 0 && out->size == renderer.paragraph_end) {
        out->size -= strlen("\\par\n");
        out->data[out->size] = '\0';
        renderer.rendered--;
    }
    add_anchor(&renderer, size);
    append(&renderer, "}");
    return (renderer.failed || out->failed) ? -1 : 0;
}

void rtf_anchors_free(struct rtf_anchors *anchors)
{
    free(anchors->items);
    anchors->items = NULL;
    anchors->count = 0;
    anchors->capacity = 0;
}

/* Index of the last anchor whose key is at or before position, or count
 * when position precedes every anchor. */
static size_t find_anchor(const struct rtf_anchors *anchors, size_t position, int by_source)
{
    size_t low = 0;
    size_t high = anchors->count;
    size_t middle;
    size_t key;

    while (low < high) {
        middle = low + (high - low) / 2;
        key = by_source ? anchors->items[middle].source : anchors->items[middle].rendered;
        if (key <= position) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low == 0 ? anchors->count : low - 1;
}

static size_t map_position(const struct rtf_anchors *anchors, size_t position, int by_source)
{
    size_t index;
    size_t from;
    size_t to;
    size_t limit;
    size_t result;

    if (anchors == NULL || anchors->count == 0) {
        return 0;
    }
    index = find_anchor(anchors, position, by_source);
    if (index == anchors->count) {
        return by_source ? anchors->items[0].rendered : anchors->items[0].source;
    }
    from = by_source ? anchors->items[index].source : anchors->items[index].rendered;
    to = by_source ? anchors->items[index].rendered : anchors->items[index].source;
    /* rtf_render ends the list with an anchor at the end of both texts, so
     * nothing maps past the last one. */
    if (index + 1 == anchors->count) {
        return to;
    }
    result = to + (position - from);
    limit = by_source ? anchors->items[index + 1].rendered : anchors->items[index + 1].source;
    return result < limit ? result : limit;
}

size_t rtf_source_to_rendered(const struct rtf_anchors *anchors, size_t source)
{
    return map_position(anchors, source, 1);
}

size_t rtf_rendered_to_source(const struct rtf_anchors *anchors, size_t rendered)
{
    return map_position(anchors, rendered, 0);
}
