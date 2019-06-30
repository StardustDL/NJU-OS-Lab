#include <common.h>
#include <buffer.h>

void buffer_init(buffer *buf, size_t cap)
{
  Assert(buf != NULL, "Buffer obj is NULL");
  buf->content = pmm->alloc(cap);
  buf->buf_cap = cap;
  buf->buf_pos = 0;
  buf->buf_size = 0;
}

void buffer_free(buffer *buf)
{
  Assert(buf != NULL, "Buffer obj is NULL");
  pmm->free(buf->content);
}

int buffer_isempty(buffer *buf)
{
  Assert(buf != NULL, "Buffer obj is NULL");
  return buf->buf_pos >= buf->buf_size;
}

int buffer_peek(buffer *buf)
{
  Assert(buf != NULL, "Buffer obj is NULL");
  if (buffer_isempty(buf))
    return -1;
  return buf->content[buf->buf_pos];
}

int buffer_next(buffer *buf)
{
  Assert(buf != NULL, "Buffer obj is NULL");
  if (buffer_isempty(buf))
    return -1;
  uint8_t res = buf->content[buf->buf_pos];
  buf->buf_pos++;
  return res;
}

int buffer_push(buffer *buf, char c)
{
  Assert(buf != NULL, "Buffer obj is NULL");
  if (buf->buf_size == buf->buf_cap)
    return -1;
  buf->content[buf->buf_size] = c;
  buf->buf_size++;
  return c;
}