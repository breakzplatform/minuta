#include "styled.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoding.h"
#include "entity.h"
#include "md4c.h"

#define MAX_LIST_DEPTH 32
#define RULE_LENGTH 24
#define CODE_INDENT "  "
#define NO_SOURCE ((size_t)-1)
#define REPLACEMENT_CHARACTER 0xFFFDUL

enum block_kind {
    KIND_TEXT,
    KIND_HEADING,
    KIND_CODE,
    KIND_HTML
};

struct list_level {
    int ordered;
    int tight;
    unsigned next_number;
    unsigned items;       /* items started so far */
    size_t marker_width;  /* width of this level's markers, for continuation lines */
};

struct table_cell {
    size_t start;   /* into table.text */
    size_t length;
    size_t source;
    unsigned row;
    MD_ALIGN align;
};

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
    struct styled_text *out;
    int failed;
    enum block_kind kind;
    unsigned heading_level;
    int block_open;
    int item_start;          /* the next block is the first one of a list item */
    int pending_newlines;    /* line breaks of verbatim text, emitted before more text */
    int quote_depth;
    int list_depth;
    struct list_level lists[MAX_LIST_DEPTH];
    char marker[24];         /* marker waiting for the item's first line */
    int bold;
    int italic;
    int code;
    int link;
    int strike;
    int underline;
    int image;
    int in_table;
    struct table table;
};

static struct buffer *text(struct renderer *r)
{
    return &r->out->text;
}

static int list_levels(const struct renderer *r)
{
    return r->list_depth < MAX_LIST_DEPTH ? r->list_depth : MAX_LIST_DEPTH;
}

static unsigned current_flags(const struct renderer *r)
{
    unsigned flags = 0;

    if (r->bold > 0 || r->kind == KIND_HEADING) {
        flags |= STYLED_BOLD;
    }
    if (r->italic > 0) {
        flags |= STYLED_ITALIC;
    }
    if (r->code > 0 || r->kind == KIND_CODE) {
        flags |= STYLED_CODE;
    }
    if (r->link > 0) {
        flags |= STYLED_LINK;
    }
    if (r->image > 0 || r->kind == KIND_HTML || (r->quote_depth > 0 && r->kind != KIND_CODE)) {
        flags |= STYLED_MUTED;
    }
    if (r->strike > 0) {
        flags |= STYLED_STRIKE;
    }
    if (r->underline > 0) {
        flags |= STYLED_UNDERLINE;
    }
    return flags;
}

/* Styles the bytes from start to the end of the text, merging with the
 * previous run when it has the same style and touches it. */
static void style_range(struct renderer *r, size_t start, unsigned flags, unsigned heading)
{
    struct styled_text *out = r->out;
    size_t end = out->text.size;
    struct styled_run *run;

    if (end <= start || (flags == 0 && heading == 0)) {
        return;
    }
    if (out->run_count > 0) {
        run = &out->runs[out->run_count - 1];
        if (run->flags == flags && run->heading == heading && run->start + run->length == start) {
            run->length = end - run->start;
            return;
        }
    }
    if (out->run_count == out->run_capacity) {
        size_t capacity = out->run_capacity < 64 ? 64 : out->run_capacity * 2;
        struct styled_run *runs = realloc(out->runs, capacity * sizeof(*runs));

        if (runs == NULL) {
            r->failed = 1;
            return;
        }
        out->runs = runs;
        out->run_capacity = capacity;
    }
    run = &out->runs[out->run_count++];
    run->start = start;
    run->length = end - start;
    run->flags = flags;
    run->heading = heading;
}

static void emit_styled(struct renderer *r, const char *bytes, size_t size, unsigned flags)
{
    size_t start = text(r)->size;

    buffer_append(text(r), bytes, size);
    style_range(r, start, flags, r->kind == KIND_HEADING ? r->heading_level : 0);
}

static void emit(struct renderer *r, const char *bytes, size_t size)
{
    emit_styled(r, bytes, size, current_flags(r));
}

static void emit_string(struct renderer *r, const char *string)
{
    emit(r, string, strlen(string));
}

static void emit_code_point(struct renderer *r, unsigned long code_point)
{
    char encoded[4];

    emit(r, encoded, utf8_encode(code_point, encoded));
}

static void add_anchor(struct renderer *r, size_t source)
{
    struct rtf_anchors *anchors = &r->out->anchors;
    size_t rendered = text(r)->size;

    if (source == NO_SOURCE) {
        return;
    }
    if (anchors->count > 0) {
        const struct rtf_anchor *last = &anchors->items[anchors->count - 1];

        if (source <= last->source || rendered < last->rendered) {
            return;
        }
    }
    if (anchors->count == anchors->capacity) {
        size_t capacity = anchors->capacity < 64 ? 64 : anchors->capacity * 2;
        struct rtf_anchor *items = realloc(anchors->items, capacity * sizeof(*items));

        if (items == NULL) {
            r->failed = 1;
            return;
        }
        anchors->items = items;
        anchors->capacity = capacity;
    }
    anchors->items[anchors->count].source = source;
    anchors->items[anchors->count].rendered = rendered;
    anchors->count++;
}

/* Quote bars and list indentation at the start of an output line. The
 * first line of a list item shows its marker; later lines are indented to
 * the marker's width. */
static void emit_prefix(struct renderer *r)
{
    int levels = list_levels(r);
    int i;

    for (i = 0; i < r->quote_depth; i++) {
        emit_styled(r, "\xE2\x94\x82 ", 4, STYLED_MUTED);
    }
    for (i = 0; i < levels; i++) {
        size_t width = r->lists[i].marker_width;

        if (i == levels - 1 && r->marker[0] != '\0') {
            emit_styled(r, r->marker, strlen(r->marker), 0);
            r->marker[0] = '\0';
        } else {
            while (width-- > 0) {
                buffer_append_char(text(r), ' ');
            }
        }
    }
    if (r->kind == KIND_CODE) {
        buffer_append_string(text(r), CODE_INDENT);
    }
}

static void new_line(struct renderer *r)
{
    buffer_append_char(text(r), '\n');
    emit_prefix(r);
}

/* Whether the block about to start opens an item of a tight list that
 * follows a sibling item or starts right inside a tight parent item: those
 * are separated by a line break only. */
static int starts_tight_item(const struct renderer *r)
{
    int levels = list_levels(r);
    const struct list_level *level;

    if (levels == 0 || !(r->item_start || r->marker[0] != '\0')) {
        return 0;
    }
    level = &r->lists[levels - 1];
    return level->tight && (level->items > 1 || (levels > 1 && r->lists[levels - 2].tight));
}

/* Starts a block: a blank line after the previous one (a line break
 * between the items of a tight list), then the line prefix. */
static void begin_block(struct renderer *r, enum block_kind kind)
{
    if (r->block_open) {
        return;
    }
    r->kind = kind;
    if (text(r)->size > 0) {
        buffer_append_char(text(r), '\n');
        if (!starts_tight_item(r)) {
            buffer_append_char(text(r), '\n');
        }
    }
    r->item_start = 0;
    emit_prefix(r);
    r->block_open = 1;
}

static void end_block(struct renderer *r)
{
    r->block_open = 0;
    r->pending_newlines = 0;
    r->kind = KIND_TEXT;
}

/* A list item whose first child is not a paragraph still shows its marker
 * on a line of its own. */
static void flush_marker(struct renderer *r)
{
    if (r->marker[0] == '\0') {
        return;
    }
    begin_block(r, KIND_TEXT);
    end_block(r);
}

/* Emits text. Verbatim text keeps its line breaks, deferred so that a
 * block's final newline is dropped; elsewhere a newline becomes a space. */
static void emit_utf8(struct renderer *r, const char *bytes, size_t size, size_t source,
                      int verbatim)
{
    size_t start = 0;
    size_t i;

    for (i = 0; i <= size; i++) {
        if (i < size && bytes[i] != '\n' && bytes[i] != '\r') {
            continue;
        }
        if (i > start) {
            while (r->pending_newlines > 0) {
                r->pending_newlines--;
                new_line(r);
            }
            if (source != NO_SOURCE) {
                add_anchor(r, source + start);
            }
            emit(r, bytes + start, i - start);
        }
        if (i < size) {
            if (bytes[i] == '\n') {
                if (verbatim) {
                    r->pending_newlines++;
                } else {
                    emit(r, " ", 1);
                }
            }
            start = i + 1;
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
static int decode_entity(const char *entity_text, size_t size, unsigned long code_points[2])
{
    const ENTITY *entity;
    unsigned long value = 0;
    size_t i;
    int digit;
    int hex;

    if (size >= 4 && entity_text[1] == '#') {
        hex = entity_text[2] == 'x' || entity_text[2] == 'X';
        for (i = hex ? 3 : 2; i < size && entity_text[i] != ';'; i++) {
            char c = entity_text[i];

            digit = -1;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (hex && c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else if (hex && c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
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
    entity = entity_lookup(entity_text, size);
    if (entity == NULL) {
        return 0;
    }
    code_points[0] = entity->codepoints[0];
    code_points[1] = entity->codepoints[1];
    return code_points[1] != 0 ? 2 : 1;
}

static size_t count_code_points(const char *bytes, size_t size)
{
    size_t count = 0;
    size_t i = 0;
    unsigned long code_point;

    while (i < size) {
        i += utf8_decode((const unsigned char *)bytes + i, size - i, &code_point);
        count++;
    }
    return count;
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
    struct table_cell *cell;

    if (table->cell_count == table->cell_capacity) {
        size_t capacity = table->cell_capacity < 16 ? 16 : table->cell_capacity * 2;
        struct table_cell *cells = realloc(table->cells, capacity * sizeof(*cells));

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

    if (table->in_cell && table->cell_count > 0) {
        struct table_cell *cell = &table->cells[table->cell_count - 1];

        cell->length = table->text.size - cell->start;
    }
    table->in_cell = 0;
}

static void table_text(struct renderer *r, MD_TEXTTYPE type, const char *bytes, size_t size,
                       size_t source)
{
    struct table *table = &r->table;
    struct table_cell *cell;
    unsigned long code_points[2];
    char encoded[4];
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
        buffer_append(&table->text, encoded, utf8_encode(REPLACEMENT_CHARACTER, encoded));
        break;
    case MD_TEXT_BR:
    case MD_TEXT_SOFTBR:
        buffer_append_char(&table->text, ' ');
        break;
    case MD_TEXT_ENTITY:
        count = decode_entity(bytes, size, code_points);
        if (count == 0) {
            buffer_append(&table->text, bytes, size);
        }
        for (i = 0; i < count; i++) {
            buffer_append(&table->text, encoded, utf8_encode(code_points[i], encoded));
        }
        break;
    default:
        buffer_append(&table->text, bytes, size);
        break;
    }
}

static void emit_padding(struct renderer *r, size_t count, unsigned flags)
{
    while (count-- > 0) {
        emit_styled(r, " ", 1, flags);
    }
}

/* Writes the collected table as monospace rows: columns padded to a common
 * width and separated by " | ", the header in bold and underlined by a row
 * of dashes. */
static void emit_table(struct renderer *r)
{
    struct table *table = &r->table;
    size_t *widths;
    MD_ALIGN *aligns;
    size_t columns = 0;
    size_t index;
    size_t first;
    size_t column;
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
    if (widths == NULL || aligns == NULL) {
        r->failed = 1;
        free(widths);
        free(aligns);
        return;
    }
    for (index = 0, first = 0; index < table->cell_count; index++) {
        size_t width;

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

    begin_block(r, KIND_TEXT);
    index = 0;
    for (row = table->cells[0].row; index < table->cell_count; row++) {
        unsigned flags = STYLED_MONOSPACE | (row == table->cells[0].row ? STYLED_BOLD : 0);

        if (row != table->cells[0].row) {
            new_line(r);
        }
        for (column = 0; column < columns; column++) {
            const struct table_cell *cell = NULL;
            size_t used = 0;
            size_t padding;
            size_t left = 0;

            if (index < table->cell_count && table->cells[index].row == row) {
                cell = &table->cells[index++];
                used = count_code_points(table->text.data + cell->start, cell->length);
            }
            if (column > 0) {
                emit_styled(r, " | ", 3, STYLED_MONOSPACE | STYLED_MUTED);
            }
            padding = widths[column] - used;
            if (aligns[column] == MD_ALIGN_RIGHT) {
                left = padding;
            } else if (aligns[column] == MD_ALIGN_CENTER) {
                left = padding / 2;
            }
            emit_padding(r, left, STYLED_MONOSPACE);
            if (cell != NULL) {
                add_anchor(r, cell->source);
                emit_styled(r, table->text.data + cell->start, cell->length, flags);
            }
            if (column + 1 < columns) {
                emit_padding(r, padding - left, STYLED_MONOSPACE);
            }
        }
        while (index < table->cell_count && table->cells[index].row == row) {
            index++;
        }
        if (row == table->cells[0].row) {
            new_line(r);
            for (column = 0; column < columns; column++) {
                size_t width;

                if (column > 0) {
                    emit_styled(r, "-|-", 3, STYLED_MONOSPACE | STYLED_MUTED);
                }
                for (width = 0; width < widths[column]; width++) {
                    emit_styled(r, "-", 1, STYLED_MONOSPACE | STYLED_MUTED);
                }
            }
        }
    }
    end_block(r);
    free(widths);
    free(aligns);
}

static size_t source_offset(const struct renderer *r, const char *bytes)
{
    if (bytes >= r->markdown && bytes < r->markdown + r->size) {
        return (size_t)(bytes - r->markdown);
    }
    return NO_SOURCE;
}

static int status(const struct renderer *r)
{
    return (r->failed || r->out->text.failed) ? -1 : 0;
}

static void begin_list(struct renderer *r, int ordered, int tight, unsigned start)
{
    r->list_depth++;
    if (r->list_depth <= MAX_LIST_DEPTH) {
        struct list_level *level = &r->lists[r->list_depth - 1];

        level->ordered = ordered;
        level->tight = tight;
        level->next_number = start;
        level->items = 0;
        level->marker_width = 2;
    }
}

static void begin_item(struct renderer *r, const MD_BLOCK_LI_DETAIL *item)
{
    struct list_level *level;
    size_t length;

    if (r->list_depth == 0) {
        return;
    }
    level = &r->lists[list_levels(r) - 1];
    level->items++;
    if (level->ordered) {
        sprintf(r->marker, "%u. ", level->next_number++);
    } else {
        strcpy(r->marker, "\xE2\x80\xA2 ");
    }
    if (item != NULL && item->is_task) {
        strcat(r->marker, item->task_mark == ' ' ? "[ ] " : "[x] ");
    }
    length = count_code_points(r->marker, strlen(r->marker));
    if (length > level->marker_width) {
        level->marker_width = length;
    }
    r->item_start = 1;
}

static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    struct renderer *r = userdata;
    int i;

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
        end_block(r);
        flush_marker(r);
        r->quote_depth++;
        break;
    case MD_BLOCK_UL:
        end_block(r);
        flush_marker(r);
        begin_list(r, 0, ((MD_BLOCK_UL_DETAIL *)detail)->is_tight, 0);
        break;
    case MD_BLOCK_OL:
        end_block(r);
        flush_marker(r);
        begin_list(r, 1, ((MD_BLOCK_OL_DETAIL *)detail)->is_tight,
                   ((MD_BLOCK_OL_DETAIL *)detail)->start);
        break;
    case MD_BLOCK_LI:
        end_block(r);
        flush_marker(r);
        begin_item(r, detail);
        break;
    case MD_BLOCK_HR:
        end_block(r);
        flush_marker(r);
        begin_block(r, KIND_TEXT);
        for (i = 0; i < RULE_LENGTH; i++) {
            emit_styled(r, "\xE2\x80\x94", 3, STYLED_MUTED);
        }
        end_block(r);
        break;
    case MD_BLOCK_H:
        r->heading_level = ((MD_BLOCK_H_DETAIL *)detail)->level;
        begin_block(r, KIND_HEADING);
        break;
    case MD_BLOCK_CODE:
        begin_block(r, KIND_CODE);
        break;
    case MD_BLOCK_HTML:
        begin_block(r, KIND_HTML);
        break;
    case MD_BLOCK_P:
        begin_block(r, KIND_TEXT);
        break;
    case MD_BLOCK_TABLE:
        end_block(r);
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
        end_block(r);
        flush_marker(r);
        r->quote_depth--;
        break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        end_block(r);
        r->list_depth--;
        break;
    case MD_BLOCK_LI:
        end_block(r);
        flush_marker(r);
        r->item_start = 0;
        break;
    case MD_BLOCK_H:
    case MD_BLOCK_CODE:
    case MD_BLOCK_HTML:
    case MD_BLOCK_P:
        end_block(r);
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
    begin_block(r, r->kind);
    switch (type) {
    case MD_SPAN_EM:
        r->italic++;
        break;
    case MD_SPAN_STRONG:
        r->bold++;
        break;
    case MD_SPAN_A:
        r->link++;
        break;
    case MD_SPAN_IMG:
        if (r->image++ == 0) {
            emit_string(r, "[image: ");
        }
        break;
    case MD_SPAN_CODE:
    case MD_SPAN_MARK:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        r->code++;
        break;
    case MD_SPAN_DEL:
        r->strike++;
        break;
    case MD_SPAN_U:
    case MD_SPAN_INS:
        r->underline++;
        break;
    case MD_SPAN_FOOTNOTE_REF:
        sprintf(label, "[%u]", ((MD_SPAN_FOOTNOTE_REF_DETAIL *)detail)->id);
        emit_string(r, label);
        break;
    default:
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
    switch (type) {
    case MD_SPAN_EM:
        r->italic--;
        break;
    case MD_SPAN_STRONG:
        r->bold--;
        break;
    case MD_SPAN_A:
        r->link--;
        break;
    case MD_SPAN_IMG:
        if (r->image == 1) {
            emit_string(r, "]");
        }
        r->image--;
        break;
    case MD_SPAN_CODE:
    case MD_SPAN_MARK:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        r->code--;
        break;
    case MD_SPAN_DEL:
        r->strike--;
        break;
    case MD_SPAN_U:
    case MD_SPAN_INS:
        r->underline--;
        break;
    default:
        break;
    }
    return status(r);
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR *bytes, MD_SIZE size, void *userdata)
{
    struct renderer *r = userdata;
    size_t source = source_offset(r, bytes);
    int verbatim = r->kind == KIND_CODE || r->kind == KIND_HTML;
    unsigned long code_points[2];
    int count;
    int i;

    if (r->in_table) {
        table_text(r, type, bytes, size, source);
        return status(r);
    }
    begin_block(r, r->kind);
    switch (type) {
    case MD_TEXT_NULLCHAR:
        add_anchor(r, source);
        emit_code_point(r, REPLACEMENT_CHARACTER);
        break;
    case MD_TEXT_BR:
        new_line(r);
        break;
    case MD_TEXT_SOFTBR:
        emit(r, " ", 1);
        break;
    case MD_TEXT_ENTITY:
        add_anchor(r, source);
        count = decode_entity(bytes, size, code_points);
        if (count == 0) {
            emit(r, bytes, size);
        }
        for (i = 0; i < count; i++) {
            emit_code_point(r, code_points[i]);
        }
        break;
    case MD_TEXT_HTML:
        if (verbatim) {
            emit_utf8(r, bytes, size, source, 1);
        } else {
            size_t start = text(r)->size;

            add_anchor(r, source);
            buffer_append(text(r), bytes, size);
            style_range(r, start, current_flags(r) | STYLED_MUTED, 0);
        }
        break;
    default:
        emit_utf8(r, bytes, size, source, verbatim);
        break;
    }
    return status(r);
}

void styled_text_init(struct styled_text *styled)
{
    buffer_init(&styled->text);
    styled->runs = NULL;
    styled->run_count = 0;
    styled->run_capacity = 0;
    styled->anchors.items = NULL;
    styled->anchors.count = 0;
    styled->anchors.capacity = 0;
}

void styled_text_free(struct styled_text *styled)
{
    buffer_free(&styled->text);
    free(styled->runs);
    rtf_anchors_free(&styled->anchors);
    styled_text_init(styled);
}

int styled_render(const char *markdown, size_t size, unsigned parser_flags,
                  struct styled_text *styled)
{
    struct renderer renderer;
    MD_PARSER parser;
    int result;

    styled_text_free(styled);
    memset(&renderer, 0, sizeof(renderer));
    renderer.markdown = markdown;
    renderer.size = size;
    renderer.out = styled;
    renderer.kind = KIND_TEXT;
    buffer_init(&renderer.table.text);

    memset(&parser, 0, sizeof(parser));
    parser.flags = parser_flags;
    parser.enter_block = enter_block;
    parser.leave_block = leave_block;
    parser.enter_span = enter_span;
    parser.leave_span = leave_span;
    parser.text = text_callback;

    result = md_parse(markdown, (MD_SIZE)size, &parser, &renderer);
    table_reset(&renderer.table);
    add_anchor(&renderer, size);
    buffer_reserve(&styled->text, 1);
    if (result != 0 || status(&renderer) != 0) {
        styled_text_free(styled);
        return -1;
    }
    return 0;
}

size_t styled_source_to_text(const struct styled_text *styled, size_t source)
{
    return rtf_source_to_rendered(&styled->anchors, source);
}

size_t styled_text_to_source(const struct styled_text *styled, size_t position)
{
    return rtf_rendered_to_source(&styled->anchors, position);
}
