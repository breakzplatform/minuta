#include "export.h"

#include <stdlib.h>
#include <string.h>

#include "md4c-html.h"

/* Kept to what Internet Explorer 4 and 5 understand. */
static const char stylesheet[] =
    "body { margin: 2em 12%; font-family: Georgia, \"Times New Roman\", serif;\n"
    "       line-height: 1.5; color: #1e1e1e; background: #f7f7f2; }\n"
    "h1, h2, h3, h4, h5, h6 { font-family: Arial, Helvetica, sans-serif; line-height: 1.2; }\n"
    "code, pre { font-family: \"Courier New\", Courier, monospace; background: #eaeae3; }\n"
    "pre { padding: 0.6em; }\n"
    "blockquote { margin-left: 0; padding-left: 1em; border-left: 3px solid #c4c4be; color: #555; }\n"
    "a { color: #1f5faf; }\n"
    "table { border-collapse: collapse; }\n"
    "th, td { border: 1px solid #c4c4be; padding: 0.2em 0.6em; }\n"
    "hr { border: 0; border-top: 1px solid #c4c4be; }\n"
    "img { max-width: 100%; }\n";

static void collect(const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    buffer_append((struct buffer *)userdata, text, size);
}

static void append_escaped_title(struct buffer *out, const char *title, enum encoding output)
{
    size_t size;
    char *utf8 = encoding_to_utf8(title, strlen(title), ENCODING_CP1252, &size);
    char *encoded;
    size_t i;

    if (utf8 == NULL) {
        out->failed = 1;
        return;
    }
    encoded = encoding_from_utf8(utf8, size, output, UNMAPPABLE_HTML_ENTITY, &size);
    free(utf8);
    if (encoded == NULL) {
        out->failed = 1;
        return;
    }
    for (i = 0; i < size; i++) {
        switch (encoded[i]) {
        case '<': buffer_append_string(out, "&lt;"); break;
        case '>': buffer_append_string(out, "&gt;"); break;
        case '&': buffer_append_string(out, "&amp;"); break;
        default: buffer_append_char(out, encoded[i]); break;
        }
    }
    free(encoded);
}

int export_html(const char *markdown, size_t size, const char *title, enum encoding output,
                struct buffer *out)
{
    struct buffer body;
    size_t bom = utf8_bom_length(markdown, size);

    buffer_init(&body);
    if (md_html(markdown + bom, (MD_SIZE)(size - bom), collect, &body, MD_DIALECT_GITHUB, 0) != 0
        || body.failed) {
        buffer_free(&body);
        return -1;
    }
    buffer_append_string(out, "<!DOCTYPE html>\n<html>\n<head>\n");
    buffer_append_string(out, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=");
    buffer_append_string(out, encoding_charset(output));
    buffer_append_string(out, "\">\n<meta name=\"generator\" content=\"Minuta\">\n<title>");
    append_escaped_title(out, title, output);
    buffer_append_string(out, "</title>\n<style type=\"text/css\">\n");
    buffer_append_string(out, stylesheet);
    buffer_append_string(out, "</style>\n</head>\n<body>\n");
    if (output == ENCODING_UTF8) {
        buffer_append(out, body.data, body.size);
    } else {
        size_t encoded_size;
        char *encoded = encoding_from_utf8(body.data == NULL ? "" : body.data, body.size, output,
                                           UNMAPPABLE_HTML_ENTITY, &encoded_size);

        if (encoded == NULL) {
            out->failed = 1;
        } else {
            buffer_append(out, encoded, encoded_size);
            free(encoded);
        }
    }
    buffer_append_string(out, "</body>\n</html>\n");
    buffer_free(&body);
    return out->failed ? -1 : 0;
}
