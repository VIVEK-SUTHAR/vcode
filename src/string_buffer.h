#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H

#include <stdlib.h>
#include <string.h>

struct string_buffer {
    char *buffer;
    int len;
};

#define STRING_BUF_INIT { NULL, 0 }

void string_buffer_append(struct string_buffer *sb, const char *s, int len);

void string_buffer_free(struct string_buffer *sb);

#endif
