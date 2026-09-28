#include "encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Upper halves (0x80-0xFF). CP1252's five undefined bytes map to the C1
 * control with the same value, like Windows does, so they round-trip. */
static const unsigned short cp1252_high[128] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

static const unsigned short cp850_high[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0,
    0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
    0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE,
    0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580,
    0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE,
    0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4,
    0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8,
    0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0
};

static int ascii_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static int name_equals(const char *a, const char *b)
{
    while (*a != '\0' && ascii_lower((unsigned char)*a) == *b) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int encoding_from_name(const char *name, enum encoding *out)
{
    if (name_equals(name, "utf8") || name_equals(name, "utf-8")) {
        *out = ENCODING_UTF8;
    } else if (name_equals(name, "1252") || name_equals(name, "cp1252")
               || name_equals(name, "windows-1252")) {
        *out = ENCODING_CP1252;
    } else if (name_equals(name, "850") || name_equals(name, "cp850")
               || name_equals(name, "ibm850")) {
        *out = ENCODING_CP850;
    } else {
        return -1;
    }
    return 0;
}

const char *encoding_charset(enum encoding encoding)
{
    switch (encoding) {
    case ENCODING_CP1252: return "windows-1252";
    case ENCODING_CP850:  return "ibm850";
    default:              return "utf-8";
    }
}

size_t utf8_bom_length(const char *text, size_t size)
{
    if (size >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB
        && (unsigned char)text[2] == 0xBF) {
        return 3;
    }
    return 0;
}

size_t utf8_decode(const unsigned char *text, size_t size, unsigned long *code_point)
{
    unsigned long value;
    unsigned long minimum;
    size_t length;
    size_t i;
    unsigned char lead = text[0];

    if (lead < 0x80) {
        *code_point = lead;
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
        value = lead & 0x1F;
        minimum = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        value = lead & 0x0F;
        minimum = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        value = lead & 0x07;
        minimum = 0x10000;
    } else {
        *code_point = 0xFFFD;
        return 1;
    }
    if (length > size) {
        *code_point = 0xFFFD;
        return 1;
    }
    for (i = 1; i < length; i++) {
        if ((text[i] & 0xC0) != 0x80) {
            *code_point = 0xFFFD;
            return 1;
        }
        value = (value << 6) | (text[i] & 0x3F);
    }
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        *code_point = 0xFFFD;
        return 1;
    }
    *code_point = value;
    return length;
}

int utf8_is_valid(const char *text, size_t size)
{
    const unsigned char *bytes = (const unsigned char *)text;
    size_t i = 0;

    while (i < size) {
        unsigned long code_point;
        size_t length;

        if (bytes[i] < 0x80) {
            i++;
            continue;
        }
        length = utf8_decode(bytes + i, size - i, &code_point);
        if (code_point == 0xFFFD && length == 1) {
            return 0;
        }
        i += length;
    }
    return 1;
}

size_t utf8_encode(unsigned long code_point, char *out)
{
    if (code_point < 0x80) {
        out[0] = (char)code_point;
        return 1;
    }
    if (code_point < 0x800) {
        out[0] = (char)(0xC0 | (code_point >> 6));
        out[1] = (char)(0x80 | (code_point & 0x3F));
        return 2;
    }
    if (code_point < 0x10000) {
        out[0] = (char)(0xE0 | (code_point >> 12));
        out[1] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code_point & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (code_point >> 18));
    out[1] = (char)(0x80 | ((code_point >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((code_point >> 6) & 0x3F));
    out[3] = (char)(0x80 | (code_point & 0x3F));
    return 4;
}

static const unsigned short *high_table(enum encoding codepage)
{
    return codepage == ENCODING_CP850 ? cp850_high : cp1252_high;
}

unsigned long codepage_to_unicode(enum encoding codepage, unsigned char byte)
{
    if (byte < 0x80) {
        return byte;
    }
    return high_table(codepage)[byte - 0x80];
}

int codepage_from_unicode(enum encoding codepage, unsigned long code_point)
{
    const unsigned short *table;
    int i;

    if (code_point < 0x80) {
        return (int)code_point;
    }
    /* Fast path: CP1252 is Latin-1 from 0xA0 up. */
    if (codepage == ENCODING_CP1252 && code_point >= 0xA0 && code_point <= 0xFF) {
        return (int)code_point;
    }
    table = high_table(codepage);
    for (i = 0; i < 128; i++) {
        if (table[i] == code_point) {
            return 0x80 + i;
        }
    }
    return -1;
}

char *encoding_to_utf8(const char *text, size_t size, enum encoding source, size_t *out_size)
{
    char *out;
    size_t length = 0;
    size_t i;

    if (source == ENCODING_UTF8) {
        out = malloc(size + 1);
        if (out == NULL) {
            return NULL;
        }
        memcpy(out, text, size);
        out[size] = '\0';
        *out_size = size;
        return out;
    }
    /* Every single byte becomes at most 3 UTF-8 bytes. */
    out = malloc(size * 3 + 1);
    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < size; i++) {
        unsigned char byte = (unsigned char)text[i];

        if (byte < 0x80) {
            out[length++] = (char)byte;
        } else {
            length += utf8_encode(codepage_to_unicode(source, byte), out + length);
        }
    }
    out[length] = '\0';
    *out_size = length;
    return out;
}

char *encoding_from_utf8(const char *text, size_t size, enum encoding target,
                         enum unmappable policy, size_t *out_size)
{
    const unsigned char *bytes = (const unsigned char *)text;
    char *out;
    size_t length = 0;
    size_t capacity;
    size_t i = 0;

    if (target == ENCODING_UTF8) {
        return encoding_to_utf8(text, size, ENCODING_UTF8, out_size);
    }
    /* Worst case: one invalid byte becomes "&#65533;" (8 bytes). */
    capacity = policy == UNMAPPABLE_HTML_ENTITY ? size * 8 + 1 : size + 1;
    out = malloc(capacity);
    if (out == NULL) {
        return NULL;
    }
    while (i < size) {
        unsigned long code_point;
        int byte;

        if (bytes[i] < 0x80) {
            out[length++] = (char)bytes[i++];
            continue;
        }
        i += utf8_decode(bytes + i, size - i, &code_point);
        byte = codepage_from_unicode(target, code_point);
        if (byte >= 0) {
            out[length++] = (char)byte;
        } else if (policy == UNMAPPABLE_HTML_ENTITY) {
            length += (size_t)sprintf(out + length, "&#%lu;", code_point);
        } else {
            out[length++] = '?';
        }
    }
    out[length] = '\0';
    *out_size = length;
    return out;
}
