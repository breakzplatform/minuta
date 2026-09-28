#ifndef MINUTA_BUFFER_H
#define MINUTA_BUFFER_H

#include <stddef.h>

/* Growable byte buffer. data is always NUL-terminated when non-NULL.
 * Once an append fails, failed is set and later appends are ignored, so
 * callers can check once at the end. */
struct buffer {
    char *data;
    size_t size;
    size_t capacity;
    int failed;
};

void buffer_init(struct buffer *buffer);
void buffer_free(struct buffer *buffer);
int buffer_reserve(struct buffer *buffer, size_t extra);
void buffer_append(struct buffer *buffer, const char *data, size_t size);
void buffer_append_string(struct buffer *buffer, const char *text);
void buffer_append_char(struct buffer *buffer, char c);
/* Detaches the data (caller frees it) and resets the buffer. */
char *buffer_take(struct buffer *buffer, size_t *size);

#endif
