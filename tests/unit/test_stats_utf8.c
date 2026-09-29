#include <string.h>

#include "check.h"
#include "stats_utf8.h"

static void count(const char *text, struct text_stats *stats)
{
    text_stats_count_utf8(text, strlen(text), stats);
}

void test_stats_utf8_counts(void)
{
    struct text_stats stats;

    count("", &stats);
    CHECK(stats.words == 0 && stats.characters == 0 && stats.reading_minutes == 0);
    /* "# Ação é\nfim" : markers alone are not words; line breaks are not characters. */
    count("# A\xC3\xA7\xC3\xA3o \xC3\xA9\nfim", &stats);
    CHECK(stats.words == 3);
    CHECK(stats.characters == 11);
    /* An em dash between spaces is not a word; CJK is. */
    count("a \xE2\x80\x94 \xE6\x97\xA5\xE6\x9C\xAC", &stats);
    CHECK(stats.words == 2 && stats.characters == 6);
    count("one\xC2\xA0two", &stats);
    CHECK(stats.words == 2);
}

void test_stats_utf8_reading_time(void)
{
    struct text_stats stats;
    char text[2001];
    int i;

    for (i = 0; i < 1000; i++) {
        text[i * 2] = 'w';
        text[i * 2 + 1] = ' ';
    }
    text[2000] = '\0';
    count(text, &stats);
    CHECK(stats.words == 1000 && stats.reading_minutes == 5);
}
