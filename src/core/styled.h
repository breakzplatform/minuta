#ifndef MINUTA_STYLED_H
#define MINUTA_STYLED_H

#include <stddef.h>

#include "buffer.h"
#include "rtf.h"

/* Markdown rendered as plain UTF-8 text plus style runs, for previews in
 * text views that take a text and a list of font/color runs (Haiku's
 * BTextView). Blocks are separated by blank lines, list items get "• " or
 * "N. " markers, quotes a "│ " bar, rules a line of em dashes, images
 * "[image: alt]", and tables become padded monospace rows. */

enum styled_flag {
    STYLED_BOLD = 0x01,
    STYLED_ITALIC = 0x02,
    STYLED_CODE = 0x04,       /* inline code and code blocks: monospace, code color */
    STYLED_LINK = 0x08,       /* link color, underlined */
    STYLED_MUTED = 0x10,      /* quotes, rules, image placeholders, raw HTML */
    STYLED_STRIKE = 0x20,
    STYLED_UNDERLINE = 0x40,
    STYLED_MONOSPACE = 0x80   /* table rows: monospace so the columns line up */
};

struct styled_run {
    size_t start;      /* byte offset into styled_text.text */
    size_t length;
    unsigned flags;    /* styled_flag values */
    unsigned heading;  /* 1-6 for heading text, 0 otherwise */
};

struct styled_text {
    struct buffer text;
    struct styled_run *runs;  /* sorted, not overlapping; unstyled text has no run */
    size_t run_count;
    size_t run_capacity;
    /* Source byte offset <-> text byte offset, for keeping the caret in
     * place when switching between the editor and the preview. */
    struct rtf_anchors anchors;
};

void styled_text_init(struct styled_text *styled);
void styled_text_free(struct styled_text *styled);

/* Renders UTF-8 Markdown. parser_flags are md4c MD_FLAG_* / MD_DIALECT_*
 * values. Replaces any previous contents. Returns 0 on success, -1 on a
 * parse error or out of memory. */
int styled_render(const char *markdown, size_t size, unsigned parser_flags,
                  struct styled_text *styled);

/* Position lookups (byte offsets on both sides). */
size_t styled_source_to_text(const struct styled_text *styled, size_t source);
size_t styled_text_to_source(const struct styled_text *styled, size_t position);

#endif
