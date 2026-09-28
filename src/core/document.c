#include "document.h"

#include <stdlib.h>
#include <string.h>

#include "buffer.h"

void document_format_init(struct document_format *format)
{
    format->encoding = ENCODING_UTF8;
    format->bom = 0;
    format->line_ending = LINE_ENDING_CRLF;
    format->preserved = NULL;
    format->preserved_count = 0;
    format->preserved_capacity = 0;
}

void document_format_free(struct document_format *format)
{
    size_t i;

    for (i = 0; i < format->preserved_count; i++) {
        free(format->preserved[i].display);
        free(format->preserved[i].original);
    }
    free(format->preserved);
    document_format_init(format);
}

static char *copy_bytes(const char *bytes, size_t size)
{
    char *copy = malloc(size + 1);

    if (copy != NULL) {
        memcpy(copy, bytes, size);
        copy[size] = '\0';
    }
    return copy;
}

static int add_preserved(struct document_format *format, const char *display, size_t display_size,
                         const char *original, size_t original_size)
{
    struct preserved_line *line;

    if (format->preserved_count == format->preserved_capacity) {
        size_t capacity = format->preserved_capacity == 0 ? 16 : format->preserved_capacity * 2;
        struct preserved_line *grown = realloc(format->preserved, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }
        format->preserved = grown;
        format->preserved_capacity = capacity;
    }
    line = &format->preserved[format->preserved_count];
    line->display = copy_bytes(display, display_size);
    line->original = copy_bytes(original, original_size);
    if (line->display == NULL || line->original == NULL) {
        free(line->display);
        free(line->original);
        return 0;
    }
    line->display_size = display_size;
    line->original_size = original_size;
    format->preserved_count++;
    return 1;
}

/* Length of the line break at position (0 if none). */
static size_t line_break_length(const char *text, size_t size, size_t position)
{
    if (text[position] == '\r') {
        return (position + 1 < size && text[position + 1] == '\n') ? 2 : 1;
    }
    return text[position] == '\n' ? 1 : 0;
}

static enum line_ending dominant_line_ending(const char *bytes, size_t size)
{
    size_t counts[3] = { 0, 0, 0 };
    size_t i = 0;

    while (i < size) {
        size_t length = line_break_length(bytes, size, i);

        if (length == 2) {
            counts[LINE_ENDING_CRLF]++;
        } else if (length == 1) {
            counts[bytes[i] == '\n' ? LINE_ENDING_LF : LINE_ENDING_CR]++;
        }
        i += length > 0 ? length : 1;
    }
    if (counts[LINE_ENDING_LF] > counts[LINE_ENDING_CRLF] && counts[LINE_ENDING_LF] >= counts[LINE_ENDING_CR]) {
        return LINE_ENDING_LF;
    }
    if (counts[LINE_ENDING_CR] > counts[LINE_ENDING_CRLF]) {
        return LINE_ENDING_CR;
    }
    return LINE_ENDING_CRLF;
}

/* Converts one UTF-8 line to CP1252, '?' for what doesn't fit. Returns 1
 * if anything was replaced. */
static int decode_utf8_line(const char *line, size_t size, struct buffer *out)
{
    const unsigned char *bytes = (const unsigned char *)line;
    int replaced = 0;
    size_t i = 0;

    while (i < size) {
        unsigned long code_point;
        int byte;

        if (bytes[i] < 0x80) {
            buffer_append_char(out, (char)bytes[i]);
            i++;
            continue;
        }
        i += utf8_decode(bytes + i, size - i, &code_point);
        byte = codepage_from_unicode(ENCODING_CP1252, code_point);
        /* CP1252's undefined bytes decode to C1 controls; don't produce them. */
        if (byte < 0 || (code_point >= 0x80 && code_point <= 0x9F)) {
            buffer_append_char(out, '?');
            replaced = 1;
        } else {
            buffer_append_char(out, (char)byte);
        }
    }
    return replaced;
}

char *document_decode(const char *bytes, size_t size, struct document_format *format,
                      size_t *text_size)
{
    struct buffer text;
    size_t bom = utf8_bom_length(bytes, size);
    size_t position;

    document_format_free(format);
    format->bom = bom > 0;
    format->encoding = (bom > 0 || utf8_is_valid(bytes, size)) ? ENCODING_UTF8 : ENCODING_CP1252;
    format->line_ending = dominant_line_ending(bytes, size);

    buffer_init(&text);
    buffer_reserve(&text, size + size / 16);
    position = bom;
    while (position <= size) {
        size_t end = position;
        size_t break_length = 0;

        while (end < size && (break_length = line_break_length(bytes, size, end)) == 0) {
            end++;
        }
        if (format->encoding == ENCODING_CP1252) {
            buffer_append(&text, bytes + position, end - position);
        } else {
            size_t line_start = text.size;

            if (decode_utf8_line(bytes + position, end - position, &text) && !text.failed
                && !add_preserved(format, text.data + line_start, text.size - line_start,
                                  bytes + position, end - position)) {
                text.failed = 1;
            }
        }
        if (end >= size) {
            break;
        }
        buffer_append(&text, "\r\n", 2);
        position = end + break_length;
    }
    if (text.failed) {
        buffer_free(&text);
        document_format_free(format);
        return NULL;
    }
    return buffer_take(&text, text_size);
}

/* Finds a preserved line showing this text, preferring the one after the
 * previous match so repeated lines keep their order. */
static const struct preserved_line *find_preserved(const struct document_format *format,
                                                   const char *line, size_t size, size_t *next)
{
    size_t count = format->preserved_count;
    size_t step;

    for (step = 0; step < count; step++) {
        size_t index = (*next + step) % count;
        const struct preserved_line *candidate = &format->preserved[index];

        if (candidate->display_size == size && memcmp(candidate->display, line, size) == 0) {
            *next = index + 1;
            return candidate;
        }
    }
    return NULL;
}

char *document_encode(const char *text, size_t size, const struct document_format *format,
                      size_t *out_size)
{
    static const char *const breaks[] = { "\r\n", "\n", "\r" };
    const char *line_break = breaks[format->line_ending];
    struct buffer out;
    size_t next_preserved = 0;
    size_t position = 0;

    buffer_init(&out);
    buffer_reserve(&out, format->encoding == ENCODING_UTF8 ? size + size / 8 : size);
    if (format->bom) {
        buffer_append(&out, "\xEF\xBB\xBF", 3);
    }
    while (position <= size) {
        size_t end = position;
        size_t break_length = 0;
        const char *line = text + position;
        size_t i;

        while (end < size && (break_length = line_break_length(text, size, end)) == 0) {
            end++;
        }
        if (format->encoding == ENCODING_CP1252) {
            buffer_append(&out, line, end - position);
        } else {
            const struct preserved_line *preserved = NULL;

            if (format->preserved_count > 0 && memchr(line, '?', end - position) != NULL) {
                preserved = find_preserved(format, line, end - position, &next_preserved);
            }
            if (preserved != NULL) {
                buffer_append(&out, preserved->original, preserved->original_size);
            } else {
                for (i = position; i < end; i++) {
                    char encoded[4];
                    unsigned char byte = (unsigned char)text[i];

                    if (byte < 0x80) {
                        buffer_append_char(&out, (char)byte);
                    } else {
                        buffer_append(&out, encoded,
                                      utf8_encode(codepage_to_unicode(ENCODING_CP1252, byte), encoded));
                    }
                }
            }
        }
        if (end >= size) {
            break;
        }
        buffer_append_string(&out, line_break);
        position = end + break_length;
    }
    if (out.failed) {
        buffer_free(&out);
        return NULL;
    }
    return buffer_take(&out, out_size);
}
