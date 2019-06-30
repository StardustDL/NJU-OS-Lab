#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>
#include <time.h>

#define MAX_SYSCALL 1024

typedef struct
{
  char name[50];
  double totalTime;
} syscallItem;

syscallItem calls[MAX_SYSCALL];

int totCall;

static time_t lastTime, nowTime;

void initList()
{
  totCall = 0;
}

void addItem(char *name, double rtime)
{
  for (int i = 0; i < totCall; i++)
  {
    if (strcmp(name, calls[i].name) == 0)
    {
      calls[i].totalTime += rtime;
      return;
    }
  }
  assert(totCall < MAX_SYSCALL);
  strcpy(calls[totCall].name, name);
  calls[totCall].totalTime = rtime;
  totCall++;
}

static int comparer(const void *a, const void *b)
{
  syscallItem *sa = (syscallItem *)a, *sb = (syscallItem *)b;
  if (sa->totalTime > sb->totalTime)
    return -1;
  if (sa->totalTime < sb->totalTime)
    return 1;
  return strcmp(sa->name, sb->name);
}

void show()
{
  static syscallItem callBuffer[MAX_SYSCALL];
  static int totBuffer;
  totBuffer = totCall;
  memcpy(callBuffer, calls, sizeof(syscallItem) * totBuffer);
  double sum = 0;
  for (int i = 0; i < totBuffer; i++)
    sum += callBuffer[i].totalTime;
  qsort(callBuffer, totBuffer, sizeof(syscallItem), comparer);
  system("clear");
  for (int i = 0; i < totBuffer; i++)
  {
    printf("%20s: %10.5lf%%\n", callBuffer[i].name, callBuffer[i].totalTime / sum * 100);
  }
  puts("");
  time(&lastTime);
}

void child(int argc, char *argv[])
{
  char *cargv[argc + 3];
  cargv[0] = "strace";
  cargv[1] = "-T";
  for (int i = 0; i < argc; i++)
  {
    cargv[i + 2] = argv[i];
  }
  cargv[argc + 2] = NULL;
  execvp("strace", cargv);
}

int isUsefulLine(const char *str, int len)
{
  return len > 0 && isalpha(str[0]) && str[len - 1] == '>';
}

int getData(const char *str, int len, char *name, double *rtime)
{
  const int LEN = 9;
  static char buffer[50];
  for (int i = 0; i < len; i++)
  {
    if (str[i] == '(')
    {
      name[i] = '\0';
      break;
    }
    name[i] = str[i];
  }
  int buf_pos = 0;
  for (int i = len - LEN; i < len; i++)
  {
    if (str[i] == '>')
    {
      buffer[buf_pos++] = '\0';
      break;
    }
    buffer[buf_pos++] = str[i];
  }
  *rtime = atof(buffer);
  return 0;
}

int parent(int pid)
{
  static char buffer[1024];
  static char nameBuffer[50];
  static double rtime;

  initList();
  time(&lastTime);
  while (scanf("%[^\n]\n", buffer) == 1)
  {
    // printf("Readed a line: %s\n", buffer);
    int len = strlen(buffer);
    if (isUsefulLine(buffer, len))
    {
      assert(getData(buffer, len, nameBuffer, &rtime) == 0);
      addItem(nameBuffer, rtime);
    }
    time(&nowTime);
    if (nowTime - lastTime >= 0.001)
    {
      show();
    }
  }
  int stat;
  if (waitpid(pid, &stat, 0) != pid)
  {
    printf("Wait process failed for errno %d.\n", errno);
    return EXIT_FAILURE;
  }
  show();
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    puts("Please give me a command to sperf.");
    return EXIT_FAILURE;
  }

  int pf[2];
  if (pipe(pf) != 0)
  {
    printf("Create pipe failed for errno %d.\n", errno);
    return EXIT_FAILURE;
  }

  int pid = fork();

  if (pid == 0)
  {
    dup2(open("/dev/null", O_RDWR), STDOUT_FILENO);
    dup2(pf[1], STDERR_FILENO);
    close(pf[0]);
    close(pf[1]);
    child(argc - 1, argv + 1);
  }
  else
  {
    dup2(pf[0], STDIN_FILENO);
    close(pf[0]);
    close(pf[1]);
    return parent(pid);
  }

  assert(0);
}
