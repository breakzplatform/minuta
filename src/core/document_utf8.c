#include "document_utf8.h"

#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "encoding.h"

/* A file without line breaks (empty, or a single line) keeps fallback, the
 * editor's own default, so that the lines typed into it get that ending. */
static enum line_ending dominant_line_ending(const char *bytes, size_t size, enum line_ending fallback)
{
    size_t counts[3] = { 0, 0, 0 };
    size_t i;

    for (i = 0; i < size; i++) {
        if (bytes[i] == '\r') {
            if (i + 1 < size && bytes[i + 1] == '\n') {
                counts[LINE_ENDING_CRLF]++;
                i++;
            } else {
                counts[LINE_ENDING_CR]++;
            }
        } else if (bytes[i] == '\n') {
            counts[LINE_ENDING_LF]++;
        }
    }
    if (counts[LINE_ENDING_LF] > counts[LINE_ENDING_CRLF] && counts[LINE_ENDING_LF] >= counts[LINE_ENDING_CR]) {
        return LINE_ENDING_LF;
    }
    if (counts[LINE_ENDING_CR] > counts[LINE_ENDING_CRLF]) {
        return LINE_ENDING_CR;
    }
    if (counts[LINE_ENDING_CRLF] == 0) {
        return fallback;
    }
    return LINE_ENDING_CRLF;
}

/* Appends text with every line break turned into "\n". */
static void append_with_newlines(struct buffer *out, const char *text, size_t size)
{
    size_t start = 0;
    size_t i;

    for (i = 0; i < size; i++) {
        if (text[i] == '\r') {
            buffer_append(out, text + start, i - start);
            buffer_append_char(out, '\n');
            if (i + 1 < size && text[i + 1] == '\n') {
                i++;
            }
            start = i + 1;
        }
    }
    buffer_append(out, text + start, size - start);
}

char *document_decode_utf8(const char *bytes, size_t size, struct document_format *format,
                           size_t *text_size)
{
    struct buffer text;
    size_t bom = utf8_bom_length(bytes, size);
    enum line_ending fallback = format->line_ending;

    document_format_free(format);
    format->bom = bom > 0;
    format->encoding = (bom > 0 || utf8_is_valid(bytes, size)) ? ENCODING_UTF8 : ENCODING_CP1252;
    format->line_ending = dominant_line_ending(bytes, size, fallback);

    buffer_init(&text);
    if (format->encoding == ENCODING_UTF8) {
        buffer_reserve(&text, size);
        append_with_newlines(&text, bytes + bom, size - bom);
    } else {
        size_t converted_size;
        char *converted = encoding_to_utf8(bytes, size, ENCODING_CP1252, &converted_size);

        if (converted == NULL) {
            return NULL;
        }
        buffer_reserve(&text, converted_size);
        append_with_newlines(&text, converted, converted_size);
        free(converted);
    }
    if (!buffer_reserve(&text, 1)) {
        text.failed = 1;
    }
    if (text.failed) {
        buffer_free(&text);
        return NULL;
    }
    return buffer_take(&text, text_size);
}

char *document_encode_utf8(const char *text, size_t size, const struct document_format *format,
                           size_t *out_size)
{
    static const char *const breaks[] = { "\r\n", "\n", "\r" };
    const char *line_break = breaks[format->line_ending];
    struct buffer out;
    size_t start = 0;
    size_t i;

    buffer_init(&out);
    if (!buffer_reserve(&out, size + size / 16 + 4)) {
        return NULL;
    }
    if (format->bom) {
        buffer_append(&out, "\xEF\xBB\xBF", 3);
    }
    for (i = 0; i <= size; i++) {
        if (i == size || text[i] == '\n') {
            buffer_append(&out, text + start, i - start);
            if (i < size) {
                buffer_append_string(&out, line_break);
            }
            start = i + 1;
        }
    }
    if (out.failed) {
        buffer_free(&out);
        return NULL;
    }
    if (format->encoding != ENCODING_UTF8) {
        size_t converted_size;
        char *converted = encoding_from_utf8(out.data, out.size, format->encoding,
                                             UNMAPPABLE_QUESTION, &converted_size);

        buffer_free(&out);
        if (converted != NULL) {
            *out_size = converted_size;
        }
        return converted;
    }
    return buffer_take(&out, out_size);
}
