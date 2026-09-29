#ifndef MINUTA_EXPORT_H
#define MINUTA_EXPORT_H

#include <stddef.h>

#include "buffer.h"
#include "encoding.h"

/* Renders UTF-8 Markdown (GitHub dialect) as a complete HTML page with a
 * small embedded stylesheet, in the given output encoding; characters that
 * encoding can't hold become &#NNNN; references. title is CP1252 text
 * (escaped here). Appends to out. Returns 0, or -1 on failure. */
int export_html(const char *markdown, size_t size, const char *title, enum encoding output,
                struct buffer *out);

#endif
