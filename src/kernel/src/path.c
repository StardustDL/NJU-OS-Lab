#include <path.h>
#include <common.h>
#include <klib.h>
#include <string.h>
#include <debug.h>

// return a new alloc string
char *path_join(const char *base, const char *child)
{
  int lb = strlen(base), lc = strlen(child);

  char *res = pmm->alloc(lb + lc + 2);

  if (lb == 0)
  {
    strcpy(res, base);
  }
  else if (lc == 0)
  {
    strcpy(res, child);
  }
  else if (base[lb - 1] == '/')
  {
    if (child[0] == '/')
    {
      sprintf(res, "%s%s", base, child + 1);
    }
    else
    {
      sprintf(res, "%s%s", base, child);
    }
  }
  else
  {
    if (child[0] == '/')
    {
      sprintf(res, "%s%s", base, child);
    }
    else
    {
      sprintf(res, "%s/%s", base, child);
    }
  }
  return res;
}

char *path_get_directory(const char *path)
{
  int len = strlen(path);
  char *buf = string_create_copy(path);
  char *res = pmm->alloc(len + 1);
  strcpy(buf, path);
  char *rp = NULL;
  if (buf[len - 1] == '/')
  {
    buf[len - 1] = '\0';
    len--;
    rp = strrchr(buf, '/');
    len++;
    buf[len - 1] = '/';
  }
  else
  {
    rp = strrchr(buf, '/');
  }

  // Assert(rp != NULL, "Not found / in path %s", buf);
  int ind = rp != NULL ? rp - buf + 1 : len;
  substr(res, buf, 0, ind);
  pmm->free(buf);
  return res;
}

char *path_get_name(const char *path)
{
  int len = strlen(path);
  char *buf = string_create_copy(path);
  char *rp = NULL;
  if (buf[len - 1] == '/')
  {
    buf[len - 1] = '\0';
    len--;
    rp = strrchr(buf, '/');
  }
  else
  {
    rp = strrchr(buf, '/');
  }

  char *res = NULL;
  // Assert(rp != NULL, "Not found / in path %s", buf);
  if (rp == NULL)
  {
    res = string_create_copy("");
  }
  else
  {
    res = string_create_copy(rp + 1);
  }
  pmm->free(buf);
  return res;
}

char *path_next_name(const char *path, const char **ptr)
{
  Assert(path != NULL, "path is NULL");

  const char *cur = path;
  *ptr = NULL;
  if (*cur == '/')
    cur++;
  int len = strlen(cur);
  if (len == 0)
  {
    return NULL;
  }

  char *next = strchr(cur, '/');
  if (next == NULL)
  {
    char *res = pmm->alloc(len + 1);
    strcpy(res, cur);
    return res;
  }
  else
  {
    *ptr = next;
    int tlen = next - cur;
    char *res = pmm->alloc(tlen + 1);
    substr(res, cur, 0, tlen);
    return res;
  }
}
