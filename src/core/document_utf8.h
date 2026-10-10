#ifndef MINUTA_DOCUMENT_UTF8_H
#define MINUTA_DOCUMENT_UTF8_H

#include <stddef.h>

#include "document.h"

/* File codec for editors that hold UTF-8 text with "\n" line breaks (the
 * Haiku port). The format records the file's encoding, BOM and dominant
 * line ending exactly as document_decode does; preserved lines are not
 * needed because UTF-8 text holds every character. */

/* Decodes file contents: UTF-8 (with a BOM, or valid UTF-8) is copied,
 * anything else is read as CP1252. Every "\r\n", "\r" or "\n" becomes "\n".
 * A file with no line break keeps the line ending already in format.
 * Returns a malloc'ed NUL-terminated buffer, or NULL when out of memory. */
char *document_decode_utf8(const char *bytes, size_t size, struct document_format *format,
                           size_t *text_size);

/* Encodes UTF-8 editor text back into the file's encoding (CP1252 gets '?'
 * for what doesn't fit) and line ending, with a BOM if the file had one.
 * Same allocation contract as decode. */
char *document_encode_utf8(const char *text, size_t size, const struct document_format *format,
                           size_t *out_size);

#endif
