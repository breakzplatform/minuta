#include <string.h>

#include "check.h"
#include "scanner.h"

/* Renders which characters of text carry flag: '^' yes, '.' no. */
static const char *mask(const char *text, struct scan_state *state, unsigned char flag)
{
    static char result[256];
    unsigned char styles[256];
    size_t size = strlen(text);
    size_t i;

    scan_line(text, size, state, styles);
    for (i = 0; i < size; i++) {
        result[i] = (styles[i] & flag) ? '^' : '.';
    }
    result[size] = '\0';
    return result;
}

static const char *line_mask(const char *text, unsigned char flag)
{
    struct scan_state state;

    scan_state_init(&state);
    return mask(text, &state, flag);
}

#define CHECK_MASK(text, flag, expected) \
    CHECK_STR(line_mask((text), (flag)), strlen(expected), (expected))

void test_scanner_heading(void)
{
    CHECK_MASK("# Title *x*", SPAN_MARKER,   "^.......^.^");
    CHECK_MASK("# Title *x*", SPAN_HEADING,  "^^^^^^^^^^^");
    CHECK_MASK("# Title *x*", SPAN_EMPHASIS, ".........^.");
    CHECK_MASK("## Closed ##", SPAN_MARKER,  "^^........^^");
    CHECK_MASK("#NoSpace", SPAN_HEADING,     "........");
    CHECK_MASK("####### seven", SPAN_HEADING, ".............");
}

void test_scanner_emphasis(void)
{
    CHECK_MASK("a **bold** b", SPAN_STRONG,   "....^^^^....");
    CHECK_MASK("a **bold** b", SPAN_MARKER,   "..^^....^^..");
    CHECK_MASK("an *it* b",    SPAN_EMPHASIS, "....^^...");
    CHECK_MASK("_under_",      SPAN_EMPHASIS, ".^^^^^.");
    CHECK_MASK("snake_case_word", SPAN_MARKER, "...............");
    CHECK_MASK("2 * 3 * 4",    SPAN_MARKER,   ".........");
    CHECK_MASK("***both***",   SPAN_STRONG,   "...^^^^...");
    CHECK_MASK("***both***",   SPAN_EMPHASIS, ".^^^^^^^^.");
    CHECK_MASK("***both***",   SPAN_MARKER,   "^^^....^^^");
    CHECK_MASK("~~gone~~",     SPAN_STRIKE,   "..^^^^..");
    CHECK_MASK("*unclosed",    SPAN_MARKER,   ".........");
}

void test_scanner_code_span(void)
{
    CHECK_MASK("x `co*de*` y", SPAN_CODE,     "..^^^^^^^^..");
    CHECK_MASK("x `co*de*` y", SPAN_MARKER,   "..^......^..");
    CHECK_MASK("x `co*de*` y", SPAN_EMPHASIS, "............");
    CHECK_MASK("``a ` b``",    SPAN_MARKER,   "^^.....^^");
    CHECK_MASK("`open",        SPAN_CODE,     ".....");
}

void test_scanner_links(void)
{
    CHECK_MASK("[link](http://x.io) t", SPAN_MARKER, "^....^^^^^^^^^^^^^^..");
    CHECK_MASK("![alt](a.png)", SPAN_MARKER, "^^...^^^^^^^^");
    CHECK_MASK("[a *b*](u_x_)", SPAN_EMPHASIS, "....^........");
    CHECK_MASK("[text][ref]", SPAN_MARKER, "^....^^^^^^");
    CHECK_MASK("[ref]: http://x", SPAN_MARKER, "^^^^^^^^^^^^^^^");
    CHECK_MASK("see <http://a.b/c> ok", SPAN_MARKER, "....^^^^^^^^^^^^^^...");
    CHECK_MASK("mail <me@x.org>", SPAN_MARKER, ".....^^^^^^^^^^");
    CHECK_MASK("a <b> c", SPAN_MARKER, ".......");
    CHECK_MASK("just [brackets]", SPAN_MARKER, "...............");
}

void test_scanner_blocks(void)
{
    CHECK_MASK("> quote", SPAN_MARKER, "^......");
    CHECK_MASK("> > deep", SPAN_MARKER, "^.^.....");
    CHECK_MASK("- item", SPAN_MARKER, "^.....");
    CHECK_MASK("  * item", SPAN_MARKER, "..^.....");
    CHECK_MASK("12. item", SPAN_MARKER, "^^^.....");
    CHECK_MASK("- [x] done", SPAN_MARKER, "^.^^^.....");
    CHECK_MASK("-notalist", SPAN_MARKER, ".........");
    CHECK_MASK("***", SPAN_MARKER, "^^^");
    CHECK_MASK("- - -", SPAN_MARKER, "^^^^^");
    CHECK_MASK("===", SPAN_MARKER, "^^^");
    CHECK_MASK("> # Quoted title", SPAN_HEADING, "..^^^^^^^^^^^^^^");
    CHECK_MASK("\\*not\\*", SPAN_MARKER, "^....^.");
    CHECK_MASK("\\*not\\*", SPAN_EMPHASIS, ".......");
}

void test_scanner_tables(void)
{
    CHECK_MASK("| a | b |", SPAN_MARKER, "^...^...^");
    CHECK_MASK("|---|:-:|", SPAN_MARKER, "^^^^^^^^^");
    CHECK_MASK("| `a|b` |", SPAN_MARKER, "^.^...^.^");
    CHECK_MASK("a | b", SPAN_MARKER, ".....");
}

void test_scanner_fences(void)
{
    struct scan_state state;
    struct scan_state shadow;
    const char *lines[] = { "```c", "code *x*", "~~~", "```", "*after*", "~~~~", "```", "~~~~~", "x" };
    const char *codes[] = { "^^^^", "^^^^^^^^", "^^^", "^^^", ".......", "^^^^", "^^^", "^^^^^", "." };
    size_t i;

    scan_state_init(&state);
    scan_state_init(&shadow);
    for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        CHECK_STR(mask(lines[i], &state, SPAN_CODE), strlen(codes[i]), codes[i]);
        scan_state_advance(&shadow, lines[i], strlen(lines[i]));
        CHECK(shadow.fence_length == state.fence_length && shadow.fence_char == state.fence_char);
    }
    CHECK(state.fence_length == 0);

    /* Replaying only the fence lines gives the same state. */
    scan_state_init(&shadow);
    for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        struct fence_info info;

        if (scan_fence_info(lines[i], strlen(lines[i]), &info)) {
            scan_state_apply_fence(&shadow, &info);
        }
        if (i == 1) {
            CHECK(shadow.fence_length == 3 && shadow.fence_char == '`');
        }
    }
    CHECK(shadow.fence_length == 0);
    CHECK(scan_is_fence_line("   ```", 6));
    CHECK(scan_is_fence_line("~~~ info", 8));
    CHECK(!scan_is_fence_line("    ```", 7));
    CHECK(!scan_is_fence_line("``` a`b", 7));
    CHECK(!scan_is_fence_line("``", 2));
}

void test_scanner_runs(void)
{
    unsigned char styles[] = { 0, 0, SPAN_MARKER, SPAN_MARKER, SPAN_STRONG };
    struct span_run run;
    size_t position = 0;

    CHECK(scan_next_run(styles, 5, &position, &run) && run.start == 0 && run.length == 2
          && run.styles == 0);
    CHECK(scan_next_run(styles, 5, &position, &run) && run.start == 2 && run.length == 2
          && run.styles == SPAN_MARKER);
    CHECK(scan_next_run(styles, 5, &position, &run) && run.start == 4 && run.length == 1);
    CHECK(!scan_next_run(styles, 5, &position, &run));
}
