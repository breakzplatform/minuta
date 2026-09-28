#ifndef MINUTA_STATS_H
#define MINUTA_STATS_H

#include <stddef.h>

#define READING_WORDS_PER_MINUTE 200

struct text_stats {
    unsigned long words;
    unsigned long characters;       /* everything except line breaks */
    unsigned long reading_minutes;  /* rounded up; 0 only for no words */
};

/* Counts CP1252 editor text. A word is a run of non-space characters with at
 * least one letter or digit, so Markdown markers such as "#" or "-" alone
 * are not words. */
void text_stats_count(const char *text, size_t size, struct text_stats *stats);

/* Finds the sentence around position in a paragraph (no line breaks inside).
 * A sentence ends after '.', '!' or '?' (plus closing quotes or brackets)
 * followed by a space. Leading spaces are not part of a sentence. */
void sentence_bounds(const char *text, size_t size, size_t position, size_t *start, size_t *end);

#endif
