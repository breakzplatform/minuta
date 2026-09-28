#include <string.h>

#include "check.h"
#include "stats.h"

void test_stats_counts(void)
{
    struct text_stats stats;
    const char *text = "# T\xEDtulo\r\n\r\n- um *dois*\r\n  tr\xEAs --- 4";

    text_stats_count(text, strlen(text), &stats);
    CHECK(stats.words == 5);
    CHECK(stats.characters == strlen(text) - 6);
    CHECK(stats.reading_minutes == 1);
    text_stats_count("", 0, &stats);
    CHECK(stats.words == 0 && stats.characters == 0 && stats.reading_minutes == 0);
}

void test_stats_reading_time(void)
{
    static char text[4000];
    struct text_stats stats;
    size_t i;

    for (i = 0; i + 2 <= sizeof(text); i += 2) {
        text[i] = 'a';
        text[i + 1] = ' ';
    }
    text_stats_count(text, sizeof(text), &stats);
    CHECK(stats.words == 2000);
    CHECK(stats.reading_minutes == 10);
    text_stats_count(text, 402, &stats);
    CHECK(stats.words == 201 && stats.reading_minutes == 2);
}

static void check_sentence(const char *text, size_t position, const char *expected)
{
    size_t start;
    size_t end;

    sentence_bounds(text, strlen(text), position, &start, &end);
    CHECK_STR(text + start, end - start, expected);
}

void test_stats_sentences(void)
{
    const char *text = "One two. Three? \"Four!\" Five";

    check_sentence(text, 0, "One two.");
    check_sentence(text, 8, "One two.");
    check_sentence(text, 9, "Three?");
    check_sentence(text, 12, "Three?");
    check_sentence(text, 18, "\"Four!\"");
    check_sentence(text, strlen(text), "Five");
    check_sentence("No end here", 3, "No end here");
    check_sentence("v1.2 is out. Yes", 2, "v1.2 is out.");
    check_sentence("", 0, "");
}
