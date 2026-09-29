#ifndef MINUTA_STATS_UTF8_H
#define MINUTA_STATS_UTF8_H

#include <stddef.h>

#include "stats.h"

/* text_stats_count for UTF-8 text: characters are code points (line breaks
 * excluded), and a word is a run of non-space characters with at least one
 * letter or digit. */
void text_stats_count_utf8(const char *text, size_t size, struct text_stats *stats);

#endif
