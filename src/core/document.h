#ifndef MINUTA_DOCUMENT_H
#define MINUTA_DOCUMENT_H

#include <stddef.h>

#include "encoding.h"

enum line_ending {
    LINE_ENDING_CRLF,
    LINE_ENDING_LF,
    LINE_ENDING_CR
};

/* A line of a UTF-8 file that had characters outside CP1252: the editor
 * shows them as '?', and saving an unchanged line writes the original bytes. */
struct preserved_line {
    char *display;
    size_t display_size;
    char *original;
    size_t original_size;
};

/* How a file was stored, so saving writes it back the same way. */
struct document_format {
    enum encoding encoding;  /* ENCODING_UTF8 or ENCODING_CP1252 */
    int bom;
    enum line_ending line_ending;
    struct preserved_line *preserved;
    size_t preserved_count;
    size_t preserved_capacity;
};

/* The format of a new document: UTF-8 without BOM, CRLF. */
void document_format_init(struct document_format *format);
void document_format_free(struct document_format *format);

/* Decodes file contents into editor text: CP1252 with "\r\n" between lines.
 * UTF-8 (with a BOM, or valid UTF-8) is converted; anything else is CP1252.
 * The line ending used most in the file becomes format->line_ending.
 * Returns a malloc'ed NUL-terminated buffer, or NULL when out of memory. */
char *document_decode(const char *bytes, size_t size, struct document_format *format,
                      size_t *text_size);

/* Encodes editor text (lines split by "\r\n", "\r" or "\n") back into the
 * file's encoding and line ending, reusing the original bytes of preserved
 * lines whose text did not change. Same allocation contract as decode. */
char *document_encode(const char *text, size_t size, const struct document_format *format,
                      size_t *out_size);

#endif
