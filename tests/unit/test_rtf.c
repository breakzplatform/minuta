#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "md4c.h"
#include "rtf.h"

static void default_theme(struct rtf_theme *theme)
{
    theme->font_name = "Courier New";
    theme->font_size = 12;
    theme->text_color = 0x00202020UL;
    theme->muted_color = 0x00808080UL;
    theme->code_background = 0x00EEEEEEUL;
    theme->link_color = 0x00CC6600UL;
}

/* Renders markdown; returns a malloc'ed NUL-terminated RTF string or NULL. */
static char *render(const char *markdown, size_t size, unsigned flags,
                    struct rtf_anchors *anchors)
{
    struct rtf_theme theme;
    struct buffer out;
    size_t length;

    default_theme(&theme);
    buffer_init(&out);
    if (rtf_render(markdown, size, flags, &theme, &out, anchors) != 0) {
        buffer_free(&out);
        return NULL;
    }
    return buffer_take(&out, &length);
}

static char *render_text(const char *markdown, unsigned flags)
{
    return render(markdown, strlen(markdown), flags, NULL);
}

static int contains(const char *rtf, const char *fragment)
{
    return rtf != NULL && strstr(rtf, fragment) != NULL;
}

static int count_occurrences(const char *rtf, const char *fragment)
{
    int count = 0;

    while (rtf != NULL && (rtf = strstr(rtf, fragment)) != NULL) {
        count++;
        rtf += strlen(fragment);
    }
    return count;
}

static int is_seven_bit(const char *rtf)
{
    while (rtf != NULL && *rtf != '\0') {
        if ((unsigned char)*rtf >= 0x80) {
            return 0;
        }
        rtf++;
    }
    return rtf != NULL;
}

void test_rtf_header(void)
{
    char *rtf = render_text("hello", 0);
    size_t size;

    CHECK(rtf != NULL && strncmp(rtf, "{\\rtf1\\ansi\\ansicpg1252\\deff0", 29) == 0);
    CHECK(contains(rtf, "{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier New;}}"));
    CHECK(contains(rtf, "{\\colortbl;\\red32\\green32\\blue32;\\red128\\green128\\blue128;"
                        "\\red238\\green238\\blue238;\\red0\\green102\\blue204;}"));
    CHECK(contains(rtf, "\\uc1"));
    CHECK(contains(rtf, "\\f0\\fs24\\cf1"));
    CHECK(contains(rtf, "\\sl300\\slmult1"));
    CHECK(contains(rtf, "{hello}"));
    /* The last paragraph has no \par of its own. */
    size = rtf != NULL ? strlen(rtf) : 0;
    CHECK(size > 2 && strcmp(rtf + size - 2, "}}") == 0);
    CHECK(!contains(rtf, "\\par\n"));
    free(rtf);
}

void test_rtf_escaping(void)
{
    /* a\b {c} é € ∞ 😀 */
    char *rtf = render_text("a\\b {c} \xC3\xA9 \xE2\x82\xAC \xE2\x88\x9E \xF0\x9F\x98\x80", 0);

    CHECK(contains(rtf, "{a\\\\b \\{c\\} \\'e9 \\'80 \\u8734? \\u-10179?\\u-8704?}"));
    CHECK(is_seven_bit(rtf));
    free(rtf);

    rtf = render("x\0y", 3, 0, NULL);
    CHECK(contains(rtf, "{x\\u-3?y}"));
    free(rtf);
}

void test_rtf_line_endings(void)
{
    char *rtf = render_text("a\r\nb  \r\nc\r\n\r\nd", 0);

    CHECK(contains(rtf, "{a b\\line c}\\par"));
    CHECK(contains(rtf, "{d}"));
    free(rtf);

    rtf = render_text("a\rb\r\rc", 0);
    CHECK(contains(rtf, "{a b}\\par"));
    CHECK(contains(rtf, "{c}"));
    free(rtf);
}

void test_rtf_headings(void)
{
    char *rtf = render_text("# One\n## Two\n### Three\n#### Four\n###### Six", 0);

    CHECK(contains(rtf, "{\\b\\fs36 One}"));
    CHECK(contains(rtf, "{\\b\\fs31 Two}"));
    CHECK(contains(rtf, "{\\b\\fs27 Three}"));
    CHECK(contains(rtf, "{\\b Four}"));
    CHECK(contains(rtf, "{\\b Six}"));
    free(rtf);
}

void test_rtf_emphasis(void)
{
    char *rtf = render_text("*i* **b** ~~s~~ `c{}`", MD_DIALECT_GITHUB);

    CHECK(contains(rtf, "{\\i i}"));
    CHECK(contains(rtf, "{\\b b}"));
    CHECK(contains(rtf, "{\\strike s}"));
    CHECK(contains(rtf, "{\\highlight3 c\\{\\}}"));
    free(rtf);

    rtf = render_text("_u_", MD_FLAG_UNDERLINE);
    CHECK(contains(rtf, "{\\ul u}"));
    free(rtf);
}

void test_rtf_code_block(void)
{
    char *rtf = render_text("```c\nx\ty\n\n  z\n```\n\nafter", 0);

    CHECK(contains(rtf, "\\sl240\\slmult1\\sb0\\sa144\\li240 {\\highlight3 x\\tab y\\line \\line   z}\\par"));
    CHECK(contains(rtf, "{after}"));
    free(rtf);
}

void test_rtf_quote(void)
{
    char *rtf = render_text("> a\n>\n> > b\n\nc", 0);

    CHECK(contains(rtf, "\\li480 {\\cf2 a}\\par"));
    CHECK(contains(rtf, "\\li960 {\\cf2 b}\\par"));
    CHECK(contains(rtf, "\\li0 {c}"));
    free(rtf);
}

void test_rtf_lists(void)
{
    char *rtf = render_text("- a\n  - b\n- [ ] t\n- [x] d\n\n3. x\n4. y\n", MD_FLAG_TASKLISTS);

    CHECK(contains(rtf, "\\li480\\fi-480\\tx480 \\'95\\tab {a}\\par"));
    CHECK(contains(rtf, "\\li960\\fi-480\\tx960 \\'95\\tab {b}\\par"));
    CHECK(contains(rtf, "\\'95\\tab [ ] {t}"));
    CHECK(contains(rtf, "\\'95\\tab [x] {d}"));
    CHECK(contains(rtf, "\\fi-480\\tx480 3.\\tab {x}"));
    CHECK(contains(rtf, "\\fi-480\\tx480 4.\\tab {y}"));
    free(rtf);

    /* Loose items: later paragraphs line up with the text, no marker. */
    rtf = render_text("1. p\n\n   q\n", 0);
    CHECK(contains(rtf, "\\li480\\fi-480\\tx480 1.\\tab {p}\\par"));
    CHECK(contains(rtf, "\\li480 {q}"));
    free(rtf);
}

void test_rtf_links(void)
{
    char *rtf = render_text("[text](http://example.com \"title\") <http://auto.example>", 0);

    CHECK(contains(rtf, "{\\ul\\cf4 text}"));
    CHECK(contains(rtf, "{\\ul\\cf4 http://auto.example}"));
    CHECK(!contains(rtf, "example.com"));
    CHECK(!contains(rtf, "title"));
    free(rtf);
}

void test_rtf_rule(void)
{
    char *rtf = render_text("a\n\n---\n\nb", 0);

    CHECK(contains(rtf, "\\qc {\\cf2 \\'97"));
    CHECK(count_occurrences(rtf, "\\'97") == 24);
    free(rtf);
}

void test_rtf_image(void)
{
    char *rtf = render_text("![alt *text*](pic.png)", 0);

    CHECK(contains(rtf, "{\\cf2 [image: alt {\\i text}]}"));
    CHECK(count_occurrences(rtf, "alt") == 1);
    CHECK(!contains(rtf, "pic.png"));
    free(rtf);
}

void test_rtf_table(void)
{
    char *rtf = render_text("| a | bb |\n|---|--:|\n| ccc | d |\n| *e* | \xC3\xA7\xC3\xA7\xC3\xA7 |\n",
                            MD_FLAG_TABLES);

    CHECK(contains(rtf, "{{\\b a   |  bb}}\\par"));
    CHECK(contains(rtf, "{----|----}\\par"));
    CHECK(contains(rtf, "{ccc |   d}\\par"));
    CHECK(contains(rtf, "{e   | \\'e7\\'e7\\'e7}"));
    free(rtf);
}

void test_rtf_entities_and_html(void)
{
    char *rtf = render_text("&amp; &copy; &#233; &#x2603; &#0; &bogus; <b>x</b>", 0);

    CHECK(contains(rtf, "{& \\'a9 \\'e9 \\u9731? \\u-3? &bogus; {\\cf2 <b>}x{\\cf2 </b>}}"));
    free(rtf);

    rtf = render_text("<div>\nhi\n</div>\n", 0);
    CHECK(contains(rtf, "{\\cf2 <div>\\line hi\\line </div>}"));
    free(rtf);
}

void test_rtf_anchors(void)
{
    const char *markdown = "# Hi\n\nab *c*";
    struct rtf_anchors anchors;
    char *rtf;

    memset(&anchors, 0, sizeof(anchors));
    rtf = render(markdown, strlen(markdown), 0, &anchors);
    CHECK(rtf != NULL);
    /* Rendered text: "Hi" \par "ab c". */
    CHECK(anchors.count == 4);
    if (anchors.count == 4) {
        CHECK(anchors.items[0].source == 2 && anchors.items[0].rendered == 0);
        CHECK(anchors.items[1].source == 6 && anchors.items[1].rendered == 3);
        CHECK(anchors.items[2].source == 10 && anchors.items[2].rendered == 6);
        CHECK(anchors.items[3].source == 12 && anchors.items[3].rendered == 7);
    }
    CHECK(rtf_source_to_rendered(&anchors, 0) == 0);
    CHECK(rtf_source_to_rendered(&anchors, 3) == 1);   /* 'i' */
    CHECK(rtf_source_to_rendered(&anchors, 7) == 4);   /* 'b' */
    CHECK(rtf_source_to_rendered(&anchors, 9) == 6);   /* '*' clamps to 'c' */
    CHECK(rtf_source_to_rendered(&anchors, 10) == 6);  /* 'c' */
    CHECK(rtf_source_to_rendered(&anchors, 99) == 7);
    CHECK(rtf_rendered_to_source(&anchors, 1) == 3);
    CHECK(rtf_rendered_to_source(&anchors, 2) == 4);
    CHECK(rtf_rendered_to_source(&anchors, 3) == 6);
    CHECK(rtf_rendered_to_source(&anchors, 5) == 8);
    CHECK(rtf_rendered_to_source(&anchors, 6) == 10);
    CHECK(rtf_rendered_to_source(&anchors, 99) == 12);
    free(rtf);

    /* Multi-byte characters: é is 2 bytes but 1 character. */
    rtf = render("\xC3\xA9xy\n\n```\nab\ncd\n```", 17, 0, &anchors);
    CHECK(rtf != NULL);
    CHECK(rtf_source_to_rendered(&anchors, 2) == 1);   /* 'x' */
    CHECK(rtf_source_to_rendered(&anchors, 3) == 2);   /* 'y' */
    CHECK(rtf_rendered_to_source(&anchors, 2) == 3);
    CHECK(rtf_source_to_rendered(&anchors, 10) == 4);  /* 'a': after "éxy" \par */
    CHECK(rtf_source_to_rendered(&anchors, 13) == 7);  /* 'c': after "ab" \line */
    CHECK(rtf_rendered_to_source(&anchors, 8) == 14);
    free(rtf);

    rtf_anchors_free(&anchors);
    CHECK(anchors.items == NULL && anchors.count == 0 && anchors.capacity == 0);
    CHECK(rtf_source_to_rendered(&anchors, 5) == 0);
    CHECK(rtf_rendered_to_source(&anchors, 5) == 0);
}

void test_rtf_anchors_monotonic(void)
{
    const char *markdown = "| a | b |\n|---|---|\n| c | d |\n\n- x\n  - y\n\n> z &amp; w";
    struct rtf_anchors anchors;
    char *rtf;
    size_t i;

    memset(&anchors, 0, sizeof(anchors));
    rtf = render(markdown, strlen(markdown), MD_FLAG_TABLES, &anchors);
    CHECK(rtf != NULL);
    CHECK(anchors.count > 6);
    for (i = 1; i < anchors.count; i++) {
        CHECK(anchors.items[i].source > anchors.items[i - 1].source);
        CHECK(anchors.items[i].rendered >= anchors.items[i - 1].rendered);
    }
    /* "a | b" \par "-----" \par "c": 'c' is character 12. */
    CHECK(rtf_source_to_rendered(&anchors, 22) == 12);
    free(rtf);
    rtf_anchors_free(&anchors);
}

void test_rtf_repeatable(void)
{
    const char *markdown = "# T\n\n- a\n\n> q\n\n| x |\n|---|\n| y |\n";
    char *first = render_text(markdown, MD_FLAG_TABLES);
    char *second = render_text(markdown, MD_FLAG_TABLES);

    CHECK(first != NULL && second != NULL && strcmp(first, second) == 0);
    free(first);
    free(second);
}
