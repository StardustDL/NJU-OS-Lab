#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>

typedef struct
{
  char content[1024];
  int len;
  int isfunc;
} input;

typedef struct __cnode
{
  char *content;
  struct __cnode *next;
} cnode;

cnode *head = NULL, *tail = NULL;

input I;

void appendContent()
{
  cnode *tmp = malloc(sizeof(cnode));
  if (head == NULL)
  {
    head = tmp;
    tail = head;
    tail->content = malloc(sizeof(char) * (I.len + 1));
    strcpy(tail->content, I.content);
    tail->next = NULL;
  }
  else
  {
    tail->next = tmp;
    tail = tmp;
    tail->content = malloc(sizeof(char) * (I.len + 1));
    strcpy(tail->content, I.content);
    tail->next = NULL;
  }
}

FILE *initTempFile(char **name, char **totalName)
{
  static char _totalName[50];
  static char raw_totalName[20] = "/tmp/crepl_XXXXXX";
  static char _name[20];
  strcpy(_totalName, raw_totalName);
  int fd = mkstemp(_totalName);
  if (fd == -1)
    return NULL;
  assert(close(fd) == 0);
  strcpy(_name, _totalName + 5);
  FILE *file = fopen(_totalName, "w");
  assert(file != NULL);
  for (cnode *cur = head; cur != NULL; cur = cur->next)
  {
    fprintf(file, "%s\n", cur->content);
  }
  *name = _name;
  *totalName = _totalName;
  return file;
}

char *appendWrapFunc(FILE *file, char *expr, char *name)
{
  static char wrap_name[50];
  sprintf(wrap_name, "__expr_wrap_%s", name);
  fprintf(file, "int %s() {\n", wrap_name);
  fprintf(file, "  return (%s);\n", expr);
  fputs("}", file);
  return wrap_name;
}

int compile(char *name, char *totalName, char **compiledName)
{
  static char buffer[256];
  static char _compiled[50];
  sprintf(_compiled, "/tmp/%s.so", name);

#if defined(__i386__)
  sprintf(buffer, "gcc -x c %s -m32 -shared -fPIC -Wall -o %s", totalName, _compiled);
#elif defined(__x86_64__)
  sprintf(buffer, "gcc -x c %s -shared -fPIC -Wall -o %s", totalName, _compiled);
#endif

  int res = system(buffer);
  *compiledName = _compiled;
  return res;
}

void *load(char *compiledName)
{
  void *handle = dlopen(compiledName, RTLD_NOW);
  return handle;
}

void *getWrap(void *handle, char *wrapName)
{
  void *res = dlsym(handle, wrapName);
  return res;
}

void prompt()
{
  printf("> ");
}

int getInput()
{
  static char func_pre[10] = "int ";
  int res = fgets(I.content, 1024, stdin) != NULL;
  if (res == 0)
    return 0;
  I.len = strlen(I.content);
  while (I.len > 0 && I.content[I.len - 1] == '\n')
    I.len--;
  I.content[I.len] = '\0';
  if (I.len < 4)
  {
    I.isfunc = 0;
  }
  else
  {
    I.isfunc = (strncmp(func_pre, I.content, 4) == 0);
  }
  return 1;
}

void execute()
{
  char *name, *totalname, *compiled, *wrapName;

  FILE *file = initTempFile(&name, &totalname);
  if (file == NULL)
  {
    puts("Create temp file Failed.");
    return;
  }

  if (I.isfunc)
  {
    fputs(I.content, file);
    fclose(file);
    if (compile(name, totalname, &compiled) == 0)
    {
      appendContent();
      printf("Added: %s\n", I.content);
    }
    else
    {
      puts("Compile failed.");
    }
  }
  else
  {

    wrapName = appendWrapFunc(file, I.content, name);

    fclose(file);

    if (compile(name, totalname, &compiled) == 0)
    {
      void *handle = load(compiled);
      if (handle == NULL)
      {
        puts("Can't load shared library.");
      }
      else
      {
        int (*wrapfunc)() = getWrap(handle, wrapName);
        if (wrapfunc == NULL)
        {
          puts("Can't load wrap function.");
        }
        else
        {
          int result = wrapfunc();
          printf("(%s) == %d \n", I.content, result);
        }
        dlclose(handle);
      }
    }
    else
    {
      puts("Compile failed.");
    }
  }
}

int main(int argc, char *argv[])
{
  while (1)
  {
    prompt();
    if (getInput() == 0)
    {
      break;
    }
    execute();
  }
  return 0;
}
