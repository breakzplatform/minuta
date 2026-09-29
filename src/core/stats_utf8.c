#include "stats_utf8.h"

#include "encoding.h"

static int is_space(unsigned long code_point)
{
    return code_point == ' ' || code_point == '\t' || code_point == '\r' || code_point == '\n'
        || code_point == 0xA0 || code_point == 0x3000
        || (code_point >= 0x2000 && code_point <= 0x200B);
}

/* Letters and digits. Above ASCII this is an approximation that is good
 * enough for counting words: Latin-1 letters and everything from U+0100 on,
 * except the punctuation and symbol blocks. */
static int is_word_character(unsigned long code_point)
{
    if ((code_point >= 'a' && code_point <= 'z') || (code_point >= 'A' && code_point <= 'Z')
        || (code_point >= '0' && code_point <= '9')) {
        return 1;
    }
    if (code_point < 0xC0) {
        return code_point == 0xAA || code_point == 0xB5 || code_point == 0xBA;
    }
    if (code_point == 0xD7 || code_point == 0xF7 || code_point == 0xFFFD) {
        return 0;
    }
    return !(code_point >= 0x2000 && code_point <= 0x2BFF)
        && !(code_point >= 0x3000 && code_point <= 0x303F);
}

void text_stats_count_utf8(const char *text, size_t size, struct text_stats *stats)
{
    const unsigned char *bytes = (const unsigned char *)text;
    int in_token = 0;
    int token_has_word = 0;
    size_t i = 0;

    stats->words = 0;
    stats->characters = 0;
    while (i < size) {
        unsigned long code_point = bytes[i];

        if (code_point < 0x80) {
            i++;
        } else {
            i += utf8_decode(bytes + i, size - i, &code_point);
        }
        if (code_point != '\r' && code_point != '\n') {
            stats->characters++;
        }
        if (is_space(code_point)) {
            if (in_token && token_has_word) {
                stats->words++;
            }
            in_token = 0;
            token_has_word = 0;
        } else {
            in_token = 1;
            token_has_word |= is_word_character(code_point);
        }
    }
    if (in_token && token_has_word) {
        stats->words++;
    }
    stats->reading_minutes = (stats->words + READING_WORDS_PER_MINUTE - 1) / READING_WORDS_PER_MINUTE;
}
