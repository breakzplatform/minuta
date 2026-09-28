#include "buffer.h"

#include <stdlib.h>
#include <string.h>

void buffer_init(struct buffer *buffer)
{
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
    buffer->failed = 0;
}

void buffer_free(struct buffer *buffer)
{
    free(buffer->data);
    buffer_init(buffer);
}

int buffer_reserve(struct buffer *buffer, size_t extra)
{
    size_t needed;
    size_t capacity;
    char *data;

    if (buffer->failed) {
        return 0;
    }
    needed = buffer->size + extra + 1;
    if (needed <= buffer->capacity) {
        return 1;
    }
    capacity = buffer->capacity < 64 ? 64 : buffer->capacity;
    while (capacity < needed) {
        capacity += capacity / 2;
    }
    data = realloc(buffer->data, capacity);
    if (data == NULL) {
        buffer->failed = 1;
        return 0;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

void buffer_append(struct buffer *buffer, const char *data, size_t size)
{
    if (!buffer_reserve(buffer, size)) {
        return;
    }
    if (size > 0) {
        memcpy(buffer->data + buffer->size, data, size);
    }
    buffer->size += size;
    buffer->data[buffer->size] = '\0';
}

void buffer_append_string(struct buffer *buffer, const char *text)
{
    buffer_append(buffer, text, strlen(text));
}

void buffer_append_char(struct buffer *buffer, char c)
{
    buffer_append(buffer, &c, 1);
}

char *buffer_take(struct buffer *buffer, size_t *size)
{
    char *data;

    if (!buffer_reserve(buffer, 0)) {
        buffer_free(buffer);
        *size = 0;
        return NULL;
    }
    data = buffer->data;
    *size = buffer->size;
    buffer_init(buffer);
    return data;
}
