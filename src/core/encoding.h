#ifndef MINUTA_ENCODING_H
#define MINUTA_ENCODING_H

#include <stddef.h>

enum encoding {
    ENCODING_UTF8,
    ENCODING_CP1252,
    ENCODING_CP850
};

/* What to emit for a code point the target code page cannot represent. */
enum unmappable {
    UNMAPPABLE_QUESTION,    /* '?' */
    UNMAPPABLE_HTML_ENTITY  /* "&#NNNN;" */
};

/* Parses "utf8"/"utf-8", "1252"/"cp1252", "850"/"cp850" (case-insensitive).
 * Returns 0 on success, -1 on an unknown name. */
int encoding_from_name(const char *name, enum encoding *out);

/* Name suitable for an HTML charset declaration ("utf-8", "windows-1252", "ibm850"). */
const char *encoding_charset(enum encoding encoding);

size_t utf8_bom_length(const char *text, size_t size);
int utf8_is_valid(const char *text, size_t size);

/* Decodes one UTF-8 sequence. Returns its length (>= 1) and the code point;
 * an invalid sequence yields length 1 and U+FFFD. */
size_t utf8_decode(const unsigned char *text, size_t size, unsigned long *code_point);

/* Encodes a code point into out (at least 4 bytes). Returns the length. */
size_t utf8_encode(unsigned long code_point, char *out);

/* Single-byte code page (CP1252 or CP850) conversions. */
unsigned long codepage_to_unicode(enum encoding codepage, unsigned char byte);
/* Returns the byte, or -1 if the code point has no representation. */
int codepage_from_unicode(enum encoding codepage, unsigned long code_point);

/* Converts text in the given encoding to UTF-8. Returns a malloc'ed,
 * NUL-terminated buffer (size in *out_size), or NULL when out of memory.
 * UTF-8 input is copied as is. */
char *encoding_to_utf8(const char *text, size_t size, enum encoding source, size_t *out_size);

/* Converts UTF-8 text to the target encoding, replacing unmappable code
 * points as requested. Same allocation contract as encoding_to_utf8. */
char *encoding_from_utf8(const char *text, size_t size, enum encoding target,
                         enum unmappable policy, size_t *out_size);

#endif
