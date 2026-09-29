#include <string.h>

#include "check.h"
#include "export.h"

void test_export_utf8(void)
{
    struct buffer out;
    const char *markdown = "# T\xC3\xADtulo\n\n*a\xC3\xA7\xC3\xA3o* \xE6\x97\xA5\n";

    buffer_init(&out);
    CHECK(export_html(markdown, strlen(markdown), "Caf\xE9 <1>", ENCODING_UTF8, &out) == 0);
    CHECK(strstr(out.data, "charset=utf-8\"") != NULL);
    CHECK(strstr(out.data, "<title>Caf\xC3\xA9 &lt;1&gt;</title>") != NULL);
    CHECK(strstr(out.data, "<h1>T\xC3\xADtulo</h1>") != NULL);
    CHECK(strstr(out.data, "<em>a\xC3\xA7\xC3\xA3o</em> \xE6\x97\xA5") != NULL);
    CHECK(strstr(out.data, "<style type=\"text/css\">") != NULL);
    buffer_free(&out);
}

void test_export_cp1252(void)
{
    struct buffer out;
    const char *markdown = "\xEF\xBB\xBF" "a\xC3\xA7\xC3\xA3o \xE6\x97\xA5 | x\n";

    buffer_init(&out);
    CHECK(export_html(markdown, strlen(markdown), "t", ENCODING_CP1252, &out) == 0);
    CHECK(strstr(out.data, "charset=windows-1252\"") != NULL);
    CHECK(strstr(out.data, "<p>a\xE7\xE3o &#26085; | x</p>") != NULL);
    buffer_free(&out);
}
