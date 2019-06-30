#include <common.h>
#include <io.h>
#include <debug.h>
#include <buffer.h>
#include <stdarg.h>

filestream *fsOpen(const char *path, int flags)
{
  int fd = vfs->open(path, flags);
  if (fd == -1)
  {
    Warning("FS open failed: %s", path);
    return NULL;
  }
  filestream *res = pmm->alloc(sizeof(filestream));
  res->fd = fd;
  res->buf = pmm->alloc(sizeof(buffer));
  res->enable_line_buf = 0;
  buffer_init(res->buf, 256);
  return res;
}

void fsClose(filestream *stream)
{
  if (stream == NULL)
  {
    Warning("FS close NULL");
    return;
  }
  fsFlush(stream);
  buffer_free(stream->buf);
  pmm->free(stream->buf);
  int cres = vfs->close(stream->fd);
  if (cres != 0)
  {
    Error("FS close failed with %d", cres);
  }
}

// return -1 if can't read more
static int continue_read(filestream *stream)
{
  if (buffer_isempty(stream->buf))
  {
    stream->buf->buf_pos = 0;
    stream->buf->buf_size = vfs->read(stream->fd, stream->buf->content, stream->buf->buf_cap);
  }
  if (buffer_isempty(stream->buf))
  {
    return -1;
  }
  return 0;
}

int fsGetChar(filestream *stream)
{
  Assert(stream != NULL, "filestream is NULL");

  if (continue_read(stream) == -1)
  {
    return -1;
  }
  else
  {
    return buffer_next(stream->buf);
  }
}

// not contain \n at tail
char *fsGetLine(filestream *stream, char *buf, int size)
{
  Assert(stream != NULL, "filestream is NULL");

  char *tbuf = buf;
  int readed = 0;
  while (readed < size)
  {
    int nc = fsGetChar(stream);
    if (nc == -1)
    {
      break;
    }
    if (nc == '\n')
    {
      readed++;
      break;
    }
    *tbuf = nc;
    tbuf++;
    readed++;
  }
  *tbuf = '\0';
  if (readed == 0)
  {
    return NULL;
  }
  else
  {
    return tbuf;
  }
}

void fsFlush(filestream *stream)
{
  if (buffer_isempty(stream->buf))
    return;
  size_t count = stream->buf->buf_size - stream->buf->buf_pos;
  size_t wrd = vfs->write(stream->fd, stream->buf->content + stream->buf->buf_pos, count);
  Assert(count == wrd, "want to flush %u, but actual %u bytes.", count, wrd);
  stream->buf->buf_pos = 0;
  stream->buf->buf_size = 0;
}

void fsPutChar(filestream *stream, char c)
{
  Assert(stream != NULL, "filestream is NULL");

  if (buffer_push(stream->buf, c) == -1)
  {
    fsFlush(stream);
    Assert(buffer_push(stream->buf, c) != -1, "push after flush failed");
  }
}

void fsPutLine(filestream *stream, const char *s)
{
  Assert(stream != NULL, "filestream is NULL");

  fsPrintf(stream, "%s\n", s);
}

int fsPrintf(filestream *stream, const char *fmt, ...)
{
  Assert(stream != NULL, "filestream is NULL");

  char bf[128];

  va_list arg;
  va_start(arg, fmt);
  int ret = vsnprintf(bf, 128, fmt, arg);
  va_end(arg);

  char *cur = bf;

  for (; *cur != '\0'; cur++)
  {
    fsPutChar(stream, *cur);
    if (stream->enable_line_buf && *cur == '\n')
      fsFlush(stream);
  }

  return ret;
}
