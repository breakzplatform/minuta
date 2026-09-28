#include "stats.h"

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Letters and digits, including CP1252's accented letters. */
static int is_word_character(char c)
{
    unsigned char byte = (unsigned char)c;

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return 1;
    }
    switch (byte) {
    case 0x83: case 0x8A: case 0x8C: case 0x8E: case 0x9A: case 0x9C: case 0x9E: case 0x9F:
    case 0xAA: case 0xB5: case 0xBA:
        return 1;
    case 0xD7: case 0xF7:
        return 0;
    default:
        return byte >= 0xC0;
    }
}

void text_stats_count(const char *text, size_t size, struct text_stats *stats)
{
    int in_token = 0;
    int token_has_word = 0;
    size_t i;

    stats->words = 0;
    stats->characters = 0;
    for (i = 0; i < size; i++) {
        char c = text[i];

        if (c != '\r' && c != '\n') {
            stats->characters++;
        }
        if (is_space(c)) {
            if (in_token && token_has_word) {
                stats->words++;
            }
            in_token = 0;
            token_has_word = 0;
        } else {
            in_token = 1;
            token_has_word |= is_word_character(c);
        }
    }
    if (in_token && token_has_word) {
        stats->words++;
    }
    stats->reading_minutes = (stats->words + READING_WORDS_PER_MINUTE - 1) / READING_WORDS_PER_MINUTE;
}

static int is_sentence_end(char c)
{
    return c == '.' || c == '!' || c == '?';
}

static int is_closing(char c)
{
    return c == '"' || c == '\'' || c == ')' || c == ']' || c == '*' || c == '_'
        || c == '\x94' || c == '\x92' || c == '\xBB';
}

/* If a sentence ends right before position (terminator, closers, then a
 * space at position), returns 1. */
static int sentence_ends_before(const char *text, size_t position)
{
    size_t i = position;

    if (i == 0 || !is_space(text[i])) {
        return 0;
    }
    while (i > 0 && is_closing(text[i - 1])) {
        i--;
    }
    return i > 0 && is_sentence_end(text[i - 1]);
}

void sentence_bounds(const char *text, size_t size, size_t position, size_t *start, size_t *end)
{
    size_t i;

    if (position > size) {
        position = size;
    }
    i = position;
    while (i < size && !sentence_ends_before(text, i)) {
        i++;
    }
    *end = i;
    i = position;
    while (i > 0 && !sentence_ends_before(text, i - 1)) {
        i--;
    }
    while (i < *end && is_space(text[i])) {
        i++;
    }
    *start = i;
}
