#include "string_buffer.h"

void string_buffer_append(struct string_buffer *sb, const char *s, int len) {
  char *new = realloc(sb->buffer, sb->len + len);
  if (new == NULL)
    return;
  memcpy(&new[sb->len], s, len);
  sb->buffer = new;
  sb->len += len;
}

void string_buffer_free(struct string_buffer *sb) { free(sb->buffer); }
