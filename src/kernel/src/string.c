#include <string.h>
#include <common.h>
#include <klib.h>
#include <debug.h>

char *string_create_copy(const char *src)
{
  int len = strlen(src);
  char *res = pmm->alloc(len + 1);
  strcpy(res, src);
  return res;
}

int atoi(const char *s, int64_t *res)
{
  *res = 0;
  int f = 1;
  for (; *s != '\0'; s++)
  {
    char c = *s;
    if (c == '-')
    {
      f = -1;
      continue;
    }
    if (c >= '0' && c <= '9')
    {
      *res = *res * 10 + c - '0';
    }
    else
    {
      return -1;
    }
  }
  *res = *res * f;
  return 0;
}