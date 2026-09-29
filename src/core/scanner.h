#ifndef MINUTA_SCANNER_H
#define MINUTA_SCANNER_H

#include <stddef.h>

/* Per-character style flags produced by the span scanner. A character can
 * carry several (e.g. a '*' inside a heading is SPAN_MARKER | SPAN_HEADING). */
enum span_style {
    SPAN_MARKER = 0x01,    /* Markdown syntax: drawn dimmed */
    SPAN_HEADING = 0x02,   /* bold */
    SPAN_EMPHASIS = 0x04,  /* italic */
    SPAN_STRONG = 0x08,    /* bold */
    SPAN_CODE = 0x10,      /* code background */
    SPAN_STRIKE = 0x20
};

/* Block state carried from one line to the next: only fenced code blocks
 * span lines. fence_length 0 means "not inside a fenced block". */
struct scan_state {
    char fence_char;
    unsigned fence_length;
};

struct span_run {
    size_t start;
    size_t length;
    unsigned char styles;
};

void scan_state_init(struct scan_state *state);

/* Scans one line (no line terminator; CP1252 or any ASCII-compatible text)
 * and fills styles[0..size) with span_style flags. Updates state for the
 * next line. */
void scan_line(const char *text, size_t size, struct scan_state *state, unsigned char *styles);

/* Updates state exactly as scan_line would, without styling. Cheap enough to
 * run over many lines; only fence lines change the state. */
void scan_state_advance(struct scan_state *state, const char *text, size_t size);

/* Returns 1 if the line could open or close a fenced code block (up to three
 * spaces, then three or more '`' or '~'). Lines for which this returns 0
 * never change a scan_state. */
int scan_is_fence_line(const char *text, size_t size);

/* What the editor remembers about a fence line so it can compute the block
 * state at any line without rereading the text in between. */
struct fence_info {
    char fence_char;
    unsigned length;
    int bare;  /* nothing but spaces after the fence: it can close a block */
};

/* Returns 1 and fills info if scan_is_fence_line would return 1. */
int scan_fence_info(const char *text, size_t size, struct fence_info *info);

/* Applies a fence line to state, exactly like scan_state_advance. */
void scan_state_apply_fence(struct scan_state *state, const struct fence_info *info);

/* Iterates over runs of equal styles: start with *position = 0 and call
 * until it returns 0. */
int scan_next_run(const unsigned char *styles, size_t size, size_t *position, struct span_run *run);

#endif
