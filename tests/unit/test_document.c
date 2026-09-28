#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "document.h"

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
    text = document_decode(bytes, size, &format, &text_size);
    CHECK_STR(text, text_size, expected_text);
    if (text_after == NULL) {
        saved = document_encode(text, text_size, &format, &saved_size);
    } else {
        saved = document_encode(text_after, strlen(text_after), &format, &saved_size);
    }
    CHECK_BYTES(saved, saved_size, expected_bytes, expected_size);
    free(text);
    free(saved);
    document_format_free(&format);
}

#define BYTES(literal) (literal), (sizeof(literal) - 1)

void test_document_cp1252(void)
{
    struct document_format format;
    char *text;
    size_t size;

    round_trip(BYTES("# Ol\xE1\r\n\r\na\xE7\xE3o\r\n"), "# Ol\xE1\r\n\r\na\xE7\xE3o\r\n", NULL,
               BYTES("# Ol\xE1\r\n\r\na\xE7\xE3o\r\n"));
    document_format_init(&format);
    text = document_decode(BYTES("caf\xE9\n"), &format, &size);
    CHECK(format.encoding == ENCODING_CP1252 && format.line_ending == LINE_ENDING_LF && !format.bom);
    free(text);
    document_format_free(&format);
}

void test_document_utf8(void)
{
    struct document_format format;
    char *text;
    size_t size;

    round_trip(BYTES("a\xC3\xA7\xC3\xA3o \xE2\x82\xAC\n"), "a\xE7\xE3o \x80\r\n", NULL,
               BYTES("a\xC3\xA7\xC3\xA3o \xE2\x82\xAC\n"));
    document_format_init(&format);
    text = document_decode(BYTES("plain\r\n"), &format, &size);
    CHECK(format.encoding == ENCODING_UTF8 && format.line_ending == LINE_ENDING_CRLF);
    free(text);
    document_format_free(&format);
}

void test_document_bom(void)
{
    round_trip(BYTES("\xEF\xBB\xBFx\r\n"), "x\r\n", NULL, BYTES("\xEF\xBB\xBFx\r\n"));
    /* A BOM wins even when the rest is not valid UTF-8. */
    round_trip(BYTES("\xEF\xBB\xBF\xE9"), "?", NULL, BYTES("\xEF\xBB\xBF\xE9"));
}

void test_document_preserves_unmappable(void)
{
    /* "日本 ok\n中?\n" : two lines with characters outside CP1252. */
    static const char original[] = "\xE6\x97\xA5\xE6\x9C\xAC ok\n\xE4\xB8\xAD?\nlast";

    round_trip(BYTES(original), "?? ok\r\n??\r\nlast", NULL, BYTES(original));
    /* Editing another line keeps the untouched ones. */
    round_trip(BYTES(original), "?? ok\r\n??\r\nlast", "?? ok\r\n??\r\nlast edited",
               BYTES("\xE6\x97\xA5\xE6\x9C\xAC ok\n\xE4\xB8\xAD?\nlast edited"));
    /* An edited line loses what it couldn't show. */
    round_trip(BYTES(original), "?? ok\r\n??\r\nlast", "?? okay\r\n??\r\nlast",
               BYTES("?? okay\n\xE4\xB8\xAD?\nlast"));
    /* Inserted lines, and RichEdit's bare "\r" breaks. */
    round_trip(BYTES(original), "?? ok\r\n??\r\nlast", "new\r?? ok\rmore\r??\rlast",
               BYTES("new\n\xE6\x97\xA5\xE6\x9C\xAC ok\nmore\n\xE4\xB8\xAD?\nlast"));
}

void test_document_repeated_lines(void)
{
    /* Two lines that both show as "?" keep their own bytes, in order. */
    round_trip(BYTES("\xE6\x97\xA5\n\xE6\x9C\xAC\n"), "?\r\n?\r\n", NULL,
               BYTES("\xE6\x97\xA5\n\xE6\x9C\xAC\n"));
}

void test_document_line_endings(void)
{
    struct document_format format;
    char *text;
    size_t size;

    round_trip(BYTES("a\rb\r"), "a\r\nb\r\n", NULL, BYTES("a\rb\r"));
    round_trip(BYTES("a\nb"), "a\r\nb", NULL, BYTES("a\nb"));
    document_format_init(&format);
    text = document_decode(BYTES("a\nb\nc\r\n"), &format, &size);
    CHECK(format.line_ending == LINE_ENDING_LF);
    free(text);
    text = document_decode(BYTES(""), &format, &size);
    CHECK(text != NULL && size == 0 && format.line_ending == LINE_ENDING_CRLF);
    free(text);
    document_format_free(&format);
}

void test_document_new_file(void)
{
    struct document_format format;
    char *saved;
    size_t size;

    document_format_init(&format);
    saved = document_encode(BYTES("# T\xEDtulo\r\ncora\xE7\xE3o"), &format, &size);
    check_bytes(__FILE__, __LINE__, saved, size, BYTES("# T\xC3\xADtulo\r\ncora\xC3\xA7\xC3\xA3o"));
    free(saved);
    document_format_free(&format);
}
