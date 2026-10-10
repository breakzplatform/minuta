#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "document_utf8.h"

#define BYTES(literal) (literal), (sizeof(literal) - 1)

/* Decodes bytes, checks the editor text, re-encodes text_after (or the
 * decoded text if NULL) and checks the saved bytes. */
static void round_trip(const char *bytes, size_t size, const char *expected_text,
                       const char *text_after, const char *expected_bytes, size_t expected_size)
{
    struct document_format format;
    char *text;
    char *saved;
    size_t text_size;
    size_t saved_size;

    document_format_init(&format);
    text = document_decode_utf8(bytes, size, &format, &text_size);
    CHECK_STR(text, text_size, expected_text);
    if (text_after == NULL) {
        saved = document_encode_utf8(text, text_size, &format, &saved_size);
    } else {
        saved = document_encode_utf8(text_after, strlen(text_after), &format, &saved_size);
    }
    CHECK_BYTES(saved, saved_size, expected_bytes, expected_size);
    free(text);
    free(saved);
    document_format_free(&format);
}

void test_document_utf8_text(void)
{
    struct document_format format;
    char *text;
    size_t size;

    round_trip(BYTES("# Ol\xC3\xA1\n\na\xC3\xA7\xC3\xA3o \xE6\x97\xA5 \xE2\x82\xAC\n"),
               "# Ol\xC3\xA1\n\na\xC3\xA7\xC3\xA3o \xE6\x97\xA5 \xE2\x82\xAC\n", NULL,
               BYTES("# Ol\xC3\xA1\n\na\xC3\xA7\xC3\xA3o \xE6\x97\xA5 \xE2\x82\xAC\n"));
    document_format_init(&format);
    text = document_decode_utf8(BYTES("x\xC3\xA9\n"), &format, &size);
    CHECK(format.encoding == ENCODING_UTF8 && format.line_ending == LINE_ENDING_LF && !format.bom);
    free(text);
    document_format_free(&format);
}

void test_document_utf8_cp1252(void)
{
    struct document_format format;
    char *text;
    size_t size;

    round_trip(BYTES("caf\xE9 \x80\r\n"), "caf\xC3\xA9 \xE2\x82\xAC\n", NULL, BYTES("caf\xE9 \x80\r\n"));
    /* What CP1252 can't hold is saved as '?'. */
    round_trip(BYTES("a\xE9\r\n"), "a\xC3\xA9\n", "a\xC3\xA9 \xE6\x97\xA5\n", BYTES("a\xE9 ?\r\n"));
    document_format_init(&format);
    text = document_decode_utf8(BYTES("caf\xE9"), &format, &size);
    CHECK(format.encoding == ENCODING_CP1252);
    free(text);
    document_format_free(&format);
}

void test_document_utf8_bom_and_endings(void)
{
    struct document_format format;
    char *text;
    size_t size;

    round_trip(BYTES("\xEF\xBB\xBFx\r\ny\r\n"), "x\ny\n", NULL, BYTES("\xEF\xBB\xBFx\r\ny\r\n"));
    round_trip(BYTES("a\rb\r"), "a\nb\n", NULL, BYTES("a\rb\r"));
    /* Mixed endings: the dominant one is used for every line. */
    round_trip(BYTES("a\nb\nc\r\n"), "a\nb\nc\n", NULL, BYTES("a\nb\nc\n"));
    round_trip(BYTES(""), "", NULL, BYTES(""));
    document_format_init(&format);
    text = document_decode_utf8(BYTES(""), &format, &size);
    CHECK(text != NULL && size == 0);
    free(text);
    document_format_free(&format);
}

void test_document_utf8_no_line_break(void)
{
    struct document_format format;
    char *text;
    char *saved;
    size_t size;

    /* An empty or one-line file keeps the editor's line ending (LF on
     * Haiku), so typed lines don't come out as CRLF. */
    document_format_init(&format);
    format.line_ending = LINE_ENDING_LF;
    text = document_decode_utf8(BYTES(""), &format, &size);
    CHECK(format.line_ending == LINE_ENDING_LF);
    free(text);
    saved = document_encode_utf8(BYTES("a\xC3\xA7\xC3\xA3o\nb\n"), &format, &size);
    CHECK_STR(saved, size, "a\xC3\xA7\xC3\xA3o\nb\n");
    free(saved);
    text = document_decode_utf8(BYTES("# T\xC3\xADtulo"), &format, &size);
    CHECK(format.line_ending == LINE_ENDING_LF);
    free(text);
    /* Any line break in the file still decides. */
    text = document_decode_utf8(BYTES("a\r\nb"), &format, &size);
    CHECK(format.line_ending == LINE_ENDING_CRLF);
    free(text);
    document_format_free(&format);
}

void test_document_utf8_new_file(void)
{
    struct document_format format;
    char *saved;
    size_t size;

    document_format_init(&format);
    format.line_ending = LINE_ENDING_LF;
    saved = document_encode_utf8(BYTES("# T\xC3\xADtulo\ncora\xC3\xA7\xC3\xA3o"), &format, &size);
    CHECK_STR(saved, size, "# T\xC3\xADtulo\ncora\xC3\xA7\xC3\xA3o");
    free(saved);
    document_format_free(&format);
}
