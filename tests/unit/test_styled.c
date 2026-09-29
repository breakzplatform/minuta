#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "md4c.h"
#include "styled.h"

static void render(const char *markdown, struct styled_text *styled)
{
    styled_text_init(styled);
    CHECK(styled_render(markdown, strlen(markdown), MD_DIALECT_GITHUB, styled) == 0);
}

static void check_text(const char *markdown, const char *expected)
{
    struct styled_text styled;

    render(markdown, &styled);
    CHECK_STR(styled.text.data, styled.text.size, expected);
    styled_text_free(&styled);
}

/* The style of the byte at the start of fragment's first occurrence. */
static const struct styled_run *run_at(const struct styled_text *styled, const char *fragment)
{
    const char *found = strstr(styled->text.data, fragment);
    size_t position;
    size_t i;

    if (found == NULL) {
        return NULL;
    }
    position = (size_t)(found - styled->text.data);
    for (i = 0; i < styled->run_count; i++) {
        const struct styled_run *run = &styled->runs[i];

        if (position >= run->start && position < run->start + run->length) {
            return run;
        }
    }
    return NULL;
}

static unsigned flags_at(const struct styled_text *styled, const char *fragment)
{
    const struct styled_run *run = run_at(styled, fragment);

    return run != NULL ? run->flags : 0;
}

void test_styled_blocks(void)
{
    check_text("# Title\n\nFirst *para*\ncontinues.\n\nSecond.\n",
               "Title\n\nFirst para continues.\n\nSecond.");
    check_text("a  \nb\n", "a\nb");
    check_text("---\n", "\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94"
               "\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94"
               "\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94"
               "\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94\xE2\x80\x94");
    check_text("```\nint x;\n\nreturn;\n```\n\nafter\n", "  int x;\n  \n  return;\n\nafter");
    check_text("> quoted\n> more\n\nout\n", "\xE2\x94\x82 quoted more\n\nout");
    check_text("![alt text](a.png)\n", "[image: alt text]");
    check_text("&copy; &#233; &bogus;\n", "\xC2\xA9 \xC3\xA9 &bogus;");
}

void test_styled_lists(void)
{
    check_text("- one\n- two\n  - nested\n- three\n\nafter\n",
               "\xE2\x80\xA2 one\n\xE2\x80\xA2 two\n  \xE2\x80\xA2 nested\n\xE2\x80\xA2 three\n\nafter");
    check_text("3. a\n4. b\n", "3. a\n4. b");
    check_text("- loose\n\n  second para\n- next\n",
               "\xE2\x80\xA2 loose\n\n  second para\n\n\xE2\x80\xA2 next");
    check_text("- [ ] todo\n- [x] done\n",
               "\xE2\x80\xA2 [ ] todo\n\xE2\x80\xA2 [x] done");
    check_text("- ```\n  code\n  ```\n", "\xE2\x80\xA2   code");
}

void test_styled_spans(void)
{
    struct styled_text styled;
    const struct styled_run *run;

    render("## Head *it*\n\nx **bold** _em_ `code` [link](http://a) ~~del~~ plain\n", &styled);
    run = run_at(&styled, "Head");
    CHECK(run != NULL && run->heading == 2 && (run->flags & STYLED_BOLD));
    run = run_at(&styled, "it");
    CHECK(run != NULL && run->heading == 2 && (run->flags & STYLED_ITALIC) && (run->flags & STYLED_BOLD));
    CHECK(flags_at(&styled, "bold") == STYLED_BOLD);
    CHECK(flags_at(&styled, "em") == STYLED_ITALIC);
    CHECK(flags_at(&styled, "code") == STYLED_CODE);
    CHECK(flags_at(&styled, "link") == STYLED_LINK);
    CHECK(flags_at(&styled, "del") == STYLED_STRIKE);
    CHECK(run_at(&styled, "plain") == NULL);
    styled_text_free(&styled);

    render("> *q*\n\n```\nc\n```\n", &styled);
    CHECK(flags_at(&styled, "q") == (STYLED_MUTED | STYLED_ITALIC));
    CHECK(flags_at(&styled, "c") == STYLED_CODE);
    styled_text_free(&styled);
}

void test_styled_table(void)
{
    struct styled_text styled;

    check_text("| a | bb |\n|---|---:|\n| ccc | d |\n",
               "a   | bb\n----|---\nccc |  d");
    render("| h |\n|---|\n| v |\n", &styled);
    CHECK(flags_at(&styled, "h") == (STYLED_BOLD | STYLED_MONOSPACE));
    CHECK(flags_at(&styled, "v") == STYLED_MONOSPACE);
    styled_text_free(&styled);
}

void test_styled_anchors(void)
{
    struct styled_text styled;
    const char *markdown = "# Title\n\nSome *text* here.\n";

    render(markdown, &styled);
    /* "Some" is at 9 in the source and 7 in the text. */
    CHECK(styled_source_to_text(&styled, 9) == 7);
    CHECK(styled_source_to_text(&styled, 11) == 9);
    CHECK(styled_text_to_source(&styled, 7) == 9);
    CHECK(styled_text_to_source(&styled, 12) == 15);   /* "text" */
    CHECK(styled_source_to_text(&styled, strlen(markdown)) == styled.text.size);
    styled_text_free(&styled);
}
