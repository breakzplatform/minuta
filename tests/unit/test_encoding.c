#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "encoding.h"

void test_encoding_names(void)
{
    enum encoding encoding;

    CHECK(encoding_from_name("UTF8", &encoding) == 0 && encoding == ENCODING_UTF8);
    CHECK(encoding_from_name("utf-8", &encoding) == 0 && encoding == ENCODING_UTF8);
    CHECK(encoding_from_name("1252", &encoding) == 0 && encoding == ENCODING_CP1252);
    CHECK(encoding_from_name("cp850", &encoding) == 0 && encoding == ENCODING_CP850);
    CHECK(encoding_from_name("latin1", &encoding) == -1);
    CHECK(strcmp(encoding_charset(ENCODING_CP1252), "windows-1252") == 0);
}

void test_encoding_bom(void)
{
    CHECK(utf8_bom_length("\xEF\xBB\xBFx", 4) == 3);
    CHECK(utf8_bom_length("\xEF\xBB", 2) == 0);
    CHECK(utf8_bom_length("abc", 3) == 0);
}

void test_encoding_utf8_validity(void)
{
    CHECK(utf8_is_valid("plain ascii", 11));
    CHECK(utf8_is_valid("a\xC3\xA7\xC3\xA3o", 6));             /* ação */
    CHECK(utf8_is_valid("\xE2\x82\xAC \xF0\x9F\x98\x80", 8));  /* € 😀 */
    CHECK(!utf8_is_valid("a\xE7\xE3o", 4));                     /* ação in CP1252 */
    CHECK(!utf8_is_valid("\xC0\xAF", 2));                       /* overlong */
    CHECK(!utf8_is_valid("\xED\xA0\x80", 3));                   /* surrogate */
    CHECK(!utf8_is_valid("\xE2\x82", 2));                       /* truncated */
    CHECK(!utf8_is_valid("\xF5\x80\x80\x80", 4));               /* > U+10FFFF */
}

void test_encoding_cp1252_round_trip(void)
{
    char all[256];
    char *utf8;
    char *back;
    size_t utf8_size;
    size_t back_size;
    int i;

    for (i = 0; i < 256; i++) {
        all[i] = (char)i;
    }
    utf8 = encoding_to_utf8(all, 256, ENCODING_CP1252, &utf8_size);
    CHECK(utf8 != NULL && utf8_is_valid(utf8, utf8_size));
    back = encoding_from_utf8(utf8, utf8_size, ENCODING_CP1252, UNMAPPABLE_QUESTION, &back_size);
    CHECK_BYTES(back, back_size, all, 256);
    free(utf8);
    free(back);

    utf8 = encoding_to_utf8("\x80 \xE7\xE3", 4, ENCODING_CP1252, &utf8_size);
    CHECK_STR(utf8, utf8_size, "\xE2\x82\xAC \xC3\xA7\xC3\xA3");
    free(utf8);
}

void test_encoding_cp850(void)
{
    char *utf8;
    char *back;
    size_t utf8_size;
    size_t back_size;

    /* "ação" in CP850: ç = 0x87, ã = 0xC6 */
    utf8 = encoding_to_utf8("a\x87\xC6o", 4, ENCODING_CP850, &utf8_size);
    CHECK_STR(utf8, utf8_size, "a\xC3\xA7\xC3\xA3o");
    back = encoding_from_utf8(utf8, utf8_size, ENCODING_CP850, UNMAPPABLE_QUESTION, &back_size);
    CHECK_STR(back, back_size, "a\x87\xC6o");
    free(utf8);
    free(back);
}

void test_encoding_unmappable(void)
{
    char *out;
    size_t size;

    /* "日€" -> CP1252 */
    out = encoding_from_utf8("\xE6\x97\xA5\xE2\x82\xAC", 6, ENCODING_CP1252, UNMAPPABLE_QUESTION, &size);
    CHECK_STR(out, size, "?\x80");
    free(out);
    out = encoding_from_utf8("\xE6\x97\xA5\xE2\x82\xAC", 6, ENCODING_CP1252, UNMAPPABLE_HTML_ENTITY, &size);
    CHECK_STR(out, size, "&#26085;\x80");
    free(out);
    /* € is not in CP850 */
    out = encoding_from_utf8("\xE2\x82\xAC", 3, ENCODING_CP850, UNMAPPABLE_HTML_ENTITY, &size);
    CHECK_STR(out, size, "&#8364;");
    free(out);
    /* an invalid byte becomes U+FFFD */
    out = encoding_from_utf8("\xFF", 1, ENCODING_CP1252, UNMAPPABLE_HTML_ENTITY, &size);
    CHECK_STR(out, size, "&#65533;");
    free(out);
}

void test_encoding_offsets(void)
{
    /* "a" "ç" "€" "b": 1 + 2 + 3 + 1 bytes in UTF-8 */
    const char *text = "a\xE7\x80" "b";
    size_t size;
    char *utf8 = encoding_to_utf8(text, 4, ENCODING_CP1252, &size);

    CHECK(size == 7);
    CHECK(cp1252_utf8_offset(text, 0) == 0);
    CHECK(cp1252_utf8_offset(text, 2) == 3);
    CHECK(cp1252_utf8_offset(text, 4) == 7);
    CHECK(utf8_cp1252_offset(utf8, size, 3) == 2);
    CHECK(utf8_cp1252_offset(utf8, size, 6) == 3);
    CHECK(utf8_cp1252_offset(utf8, size, 99) == 4);
    free(utf8);
}

void test_encoding_entity_worst_case(void)
{
    char *out;
    size_t size;

    /* Every invalid byte expands to 8 output bytes, the sizing worst case. */
    out = encoding_from_utf8("\xFF\xFE\x80", 3, ENCODING_CP1252, UNMAPPABLE_HTML_ENTITY, &size);
    CHECK_STR(out, size, "&#65533;&#65533;&#65533;");
    free(out);
    /* A truncated sequence at the end decodes byte by byte. */
    out = encoding_from_utf8("a\xE6\x97", 3, ENCODING_CP1252, UNMAPPABLE_HTML_ENTITY, &size);
    CHECK_STR(out, size, "a&#65533;&#65533;");
    free(out);
    /* U+10FFFF: 4 input bytes, 10 output bytes. */
    out = encoding_from_utf8("\xF4\x8F\xBF\xBF", 4, ENCODING_CP850, UNMAPPABLE_HTML_ENTITY, &size);
    CHECK_STR(out, size, "&#1114111;");
    free(out);
}
