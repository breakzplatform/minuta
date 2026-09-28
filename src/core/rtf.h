#ifndef MINUTA_RTF_H
#define MINUTA_RTF_H

#include <stddef.h>

#include "buffer.h"

/* Colors are Win32 COLORREF values: 0x00BBGGRR. */
struct rtf_theme {
    const char *font_name;          /* one font for everything, e.g. "Courier New" */
    int font_size;                  /* body size in points */
    unsigned long text_color;
    unsigned long muted_color;      /* quotes, rules, image placeholders, raw HTML */
    unsigned long code_background;  /* inline code and code blocks */
    unsigned long link_color;
};

/* Maps a position in the Markdown source to the matching position in the
 * text RichEdit shows for the rendered RTF, so the caret can move between
 * the editor and the preview. */
struct rtf_anchor {
    size_t source;    /* byte offset into the Markdown input */
    size_t rendered;  /* character position in the rendered document */
};

struct rtf_anchors {
    struct rtf_anchor *items;  /* sorted by source and by rendered offset */
    size_t count;
    size_t capacity;
};

/* Renders UTF-8 Markdown to RTF (\ansicpg1252; characters outside CP1252 as
 * \uN?). parser_flags are md4c MD_FLAG_* / MD_DIALECT_* values. anchors may
 * be NULL. Returns 0 on success, -1 on a parse error or out of memory. */
int rtf_render(const char *markdown, size_t size, unsigned parser_flags,
               const struct rtf_theme *theme, struct buffer *out,
               struct rtf_anchors *anchors);

void rtf_anchors_free(struct rtf_anchors *anchors);

/* Position lookups; both return 0 for an empty list. */
size_t rtf_source_to_rendered(const struct rtf_anchors *anchors, size_t source);
size_t rtf_rendered_to_source(const struct rtf_anchors *anchors, size_t rendered);

#endif
