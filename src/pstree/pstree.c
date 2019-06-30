#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

// #define DEBUG

#ifdef DEBUG
#include <common/debug.h>
#endif

typedef uint8_t bool;

#define true 1
#define false 0

#define MAX_PROC 512
const char *PROC = "/proc";

typedef struct
{
  int pid;
  char comm[256];
  char state;
  int ppid;
} Process;


/**
 * All CLI options
 */
typedef struct
{
  bool show_pids;
  bool version;
  bool numeric_sort;
} Options;
Options cli;

char procIds[MAX_PROC][256];
int procNumber = 0;
Process procs[MAX_PROC];

bool showd[MAX_PROC];

int getProcessFolders();
int getProcess(char *, Process *);
void showTree(int);

int main(int argc, char *argv[])
{
  cli.show_pids = false;
  cli.version = false;
  cli.numeric_sort = false;

  {
    int i;
    for (i = 0; i < argc; i++)
    {
      assert(argv[i]); // always true
      if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--show-pids") == 0)
        cli.show_pids = true;
      if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0)
        cli.version = true;
      if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric-sort") == 0)
        cli.numeric_sort = true;
    }
    assert(!argv[argc]); // always true
  }

  if (cli.version)
  {
    printf("pstree 1.0\nCopyright (C) 2019-2019 StardustDL\n");
    return EXIT_SUCCESS;
  }

  procNumber = getProcessFolders();
  // Assert(procNumber >= 0, "get process folder Failed");
  // Info("Founded %d process.", procNumber);

  for (int i = 0; i < procNumber; i++)
  {
    int ok = getProcess(procIds[i], procs + i);
    if (ok != 0)
    {
      procs[i].pid = -1;
      // Warning("Process get failed: %s", procIds[i]);
    }
  }

  for (int i = 0; i < procNumber; i++)
    showd[i] = false;

  for (int i = 0; i < procNumber; i++)
    if (procs[i].ppid == 0)
      showTree(i);

#ifdef DEBUG
  for (int i = 0; i < procNumber; i++)
  {
    if (showd[i] == false)
    {
      Warning("The process did not show: %d %s", procs[i].pid, procs[i].comm);
    }
  }
#endif

  return EXIT_SUCCESS;
}

extern int errno;

/**
 * Gets all process in /proc/
 * Return the number of processes.
 * Return -1 if error occurs
 */
int getProcessFolders()
{
  DIR *dir = opendir(PROC);
  struct dirent *file;
  if (NULL == dir)
    return -1;
  int len = 0;
  while (NULL != (file = readdir(dir)))
  {
    if (strncmp(file->d_name, ".", 1) == 0)
      continue;
    int l = strlen(file->d_name);
    bool flg = true;
    for (int i = 0; i < l; i++)
      if (!isdigit(file->d_name[i]))
      {
        flg = false;
        break;
      }
    if (!flg)
      continue;

    strcpy(procIds[len++], file->d_name);
  }

#ifdef DEBUG
  for (int i = 0; i < len; i++)
    Log("%s", procIds[i]);
#endif

  closedir(dir);

  return len;
}

/**
 * Gets process by id
 * Return -1 if error occurs
 */
int getProcess(char *id, Process *out)
{
  static char tmppath[512];
  static char buf[1024];
  static char tmp[128];
  {
    sprintf(tmppath, "%s/%s/stat", PROC, id);
    FILE *stat = fopen(tmppath, "rt");
    if (NULL == stat)
      return -1;

    fgets(buf, sizeof(buf), stat);

    fclose(stat);

    // Info("%s", buf);

    sscanf(buf, "%d %s %c %d", &out->pid, tmp, &out->state, &out->ppid);
    strcpy(out->comm, tmp + 1);
    out->comm[strlen(out->comm) - 1] = '\0';

    // Info("%p", stat);
  }
  return 0;
}

int cmpByName(const void *a, const void *b)
{
  int i = *((int *)a), j = *((int *)b);
  return strcmp(procs[i].comm, procs[j].comm);
}
int cmpByPid(const void *a, const void *b)
{
  int i = *((int *)a), j = *((int *)b);
  if (procs[i].pid > procs[j].pid)
    return 1;
  else if (procs[i].pid < procs[j].pid)
    return -1;
  else
    return 0;
}

void showTree(int rid)
{
  static int offset = 0;
  static char tmp[512];
  int children[MAX_PROC], chLen = 0;
  Process *root = &procs[rid];
  for (int i = 0; i < offset; i++)
    putchar(' ');
  if (cli.show_pids)
  {
    sprintf(tmp, "%s(%d)", root->comm, root->pid);
  }
  else
  {
    sprintf(tmp, "%s", root->comm);
  }
  puts(tmp);
  showd[rid] = true;
  int delta = 4;
  offset += delta;

  for (int pi = 0; pi < procNumber; pi++)
  {
    if (procs[pi].pid == -1)
      continue;
    if (procs[pi].ppid != root->pid)
      continue;
    children[chLen++] = pi;
  }

  if (cli.numeric_sort)
  {
    qsort(children, chLen, sizeof(int), cmpByPid);
  }
  else
  {
    qsort(children, chLen, sizeof(int), cmpByName);
  }

  for (int i = 0; i < chLen; i++)
    showTree(children[i]);

  offset -= delta;
}