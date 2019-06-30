#ifndef __SHELL_H__
#define __SHELL_H__

#include <common.h>
#include <io.h>
#include <buffer.h>

#define N_CMD_HANDLERS 14

typedef struct
{
  const char *name;
  filestream *stdin;
  filestream *stdout;
  buffer *cwd;
} shell_data;

#define N_ARGS 16

typedef struct
{
  char *head;
  char *args[N_ARGS];
  int argc;
  const char* stdin_path;
  const char* stderr_path;
  const char* stdout_path;
  filestream *stdin;
  filestream *stdout;
  filestream *stderr;
} command;

typedef int (*cmd_handler_func)(shell_data *sh, command* cmd);

typedef struct
{
  const char *head;
  cmd_handler_func handler;
} command_handler;

command_handler *get_command_handlers();

#endif
