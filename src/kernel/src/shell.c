#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <common.h>
#include <klib.h>
#include <debug.h>
#include <io.h>
#include <buffer.h>
#include <path.h>
#include <fs.h>
#include <string.h>
#include <shell.h>
#include <tty.h>

#define DEV

#define BUF_CAP 128

static char *path_expand(shell_data *sh, const char *path)
{
  char *target_dir = NULL;
  if (streq(path, "."))
  {
    target_dir = string_create_copy(sh->cwd->content);
  }
  else if (streq(path, ".."))
  {
    target_dir = path_get_directory(sh->cwd->content);
  }
  else if (strneq(path, "/", 1))
  {
    target_dir = string_create_copy(path);
  }
  else
  {
    target_dir = path_join(sh->cwd->content, path);
  }
  return target_dir;
}

int cmd_hello(shell_data *sh, command *cmd)
{
  fsPutLine(cmd->stdout, "Hello world from StardustDL's shell!");
  return 0;
}

int cmd_pwd(shell_data *sh, command *cmd)
{
  fsPutLine(cmd->stdout, sh->cwd->content);
  return 0;
}

int cmd_cd(shell_data *sh, command *cmd)
{
  if (cmd->argc != 1)
  {
    fsPutLine(cmd->stderr, "cd needs 1 argument.");
    return -1;
  }
  char *target_dir = path_expand(sh, cmd->args[0]);

  int res = vfs->access(target_dir, DIR_EXIST);

  if (res == 0)
  {
    strcpy(sh->cwd->content, target_dir);
    pmm->free(target_dir);
    return 0;
  }
  else
  {
    fsPrintf(sh->stdout, "Can't access the directory %s\n", target_dir);
    pmm->free(target_dir);
    return -1;
  }
}

int cmd_ls(shell_data *sh, command *cmd)
{
  char *filepath = NULL;

  if (cmd->argc == 0)
  {
    filepath = sh->cwd->content;
  }
  else if (cmd->argc == 1)
  {
    filepath = path_expand(sh, cmd->args[0]);
  }
  else
  {
    fsPutLine(cmd->stderr, "ls needs 0 or 1 argument.");
    return -1;
  }

  int res = vfs->access(filepath, DIR_EXIST);

  if (res == 0)
  {
    fslist *fl = vfs->readdir(filepath);

    int cnt = 0;

    while (fl != NULL)
    {
      fslist *nxt = fl->next;
      int align = 20 - strlen(fl->name);
      switch (fl->type)
      {
      case ITEM_DIR:
        fsPrintf(cmd->stdout, "D  %s", fl->name);
        break;
      case ITEM_FILE:
        fsPrintf(cmd->stdout, "F  %s", fl->name);
        break;
      case ITEM_LINK:
        fsPrintf(cmd->stdout, "L  %s", fl->name);
        break;
      default:
        Warning("No this type %d for %s", fl->type, fl->name);
        break;
      }
      for (int i = 0; i < align; i++)
        fsPutChar(cmd->stdout, ' ');

      fslist_node_free(fl);
      fl = nxt;

      cnt++;
      if (cnt % 2 == 0)
        fsPutChar(cmd->stdout, '\n');
    }

    if (cnt % 2 == 1)
      fsPutChar(cmd->stdout, '\n');
  }
  else
  {
    fsPrintf(cmd->stderr, "Can't access the directory %s\n", filepath);
  }

  if (cmd->argc == 1)
  {
    pmm->free(filepath);
  }

  return res;
}

int cmd_cat(shell_data *sh, command *cmd)
{
  if (cmd->argc != 0)
  {
    fsPutLine(cmd->stderr, "cat needs 0 argument.");
    return -1;
  }

  filestream *fs = cmd->stdin;

  int ch = fsGetChar(fs);

  int endchar = -1;

  if (sh->stdin == cmd->stdin)
  {
    endchar = tty_char_ctrlD();
  }

  while (ch != endchar)
  {
    fsPutChar(cmd->stdout, ch);
    ch = fsGetChar(fs);
  }
  if (sh->stdin == cmd->stdin)
  {
    fsGetChar(fs); // read the end \n
  }
  fsFlush(cmd->stdout);

  return 0;
}

int cmd_echo(shell_data *sh, command *cmd)
{
  if (cmd->argc == 1)
  {
    fsPutLine(cmd->stdout, cmd->args[0]);
  }
  else
  {
    fsPutLine(cmd->stderr, "echo needs 1 argument.");
    return -1;
  }
  return 0;
}

int cmd_mkdir(shell_data *sh, command *cmd)
{
  if (cmd->argc != 1)
  {
    fsPutLine(cmd->stderr, "mkdir needs 1 argument.");
    return -1;
  }

  char *filepath = path_join(sh->cwd->content, cmd->args[0]);

  int res = vfs->mkdir(filepath);

  if (res != 0)
  {
    fsPrintf(cmd->stderr, "mkdir %s failed.\n", filepath);
  }

  pmm->free(filepath);

  return res;
}

int cmd_rm(shell_data *sh, command *cmd)
{
  if (cmd->argc != 1)
  {
    fsPutLine(cmd->stderr, "rm needs 1 argument.");
    return -1;
  }

  char *filepath = path_join(sh->cwd->content, cmd->args[0]);

  int res = vfs->rm(filepath);

  if (res != 0)
  {
    fsPrintf(cmd->stderr, "rm %s failed.\n", filepath);
  }

  pmm->free(filepath);

  return res;
}

int cmd_ln(shell_data *sh, command *cmd)
{
  if (cmd->argc != 2)
  {
    fsPutLine(cmd->stderr, "ln needs 2 argument.");
    return -1;
  }

  char *old = path_expand(sh, cmd->args[0]);
  char *new = path_expand(sh, cmd->args[1]);

  int res = vfs->link(old, new);

  if (res != 0)
  {
    fsPrintf(cmd->stderr, "ln %s %s failed.\n", old, new);
  }

  pmm->free(old);
  pmm->free(new);

  return res;
}

#pragma region command function

enum redirectio_type
{
  R_NONE,
  R_STDIN,
  R_STDOUT,
  R_STDERR,
};

// split by space
int command_parse(shell_data *sh, command *cmd, const char *s)
{
  Assert(cmd != NULL, "target command is NULL");
  cmd->head = NULL;
  cmd->argc = 0;
  cmd->stderr = NULL;
  cmd->stdin = NULL;
  cmd->stdout = NULL;
  cmd->stderr_path = NULL;
  cmd->stdin_path = NULL;
  cmd->stdout_path = NULL;
  const char *t = s;

  int lastRedirect = R_NONE;

  while (t != NULL)
  {
    char *tspace = strchr(t, ' ');
    char *item = pmm->alloc(32);
    if (tspace == NULL)
    {
      strcpy(item, t);
      t = NULL;
    }
    else
    {
      int tlen = tspace - t;
      substr(item, t, 0, tlen);
      t = tspace + 1;
    }
    int litem = strlen(item);
    if (litem > 0)
    {
      if (streq(item, "<"))
      {
        if (lastRedirect != R_NONE)
        {
          Warning("double redirect defs");
        }
        else
        {
          lastRedirect = R_STDIN;
        }
      }
      else if (streq(item, ">") || streq(item, "1>"))
      {
        if (lastRedirect != R_NONE)
        {
          Warning("double redirect defs");
        }
        else
        {
          lastRedirect = R_STDOUT;
        }
      }
      else if (streq(item, "2>"))
      {
        if (lastRedirect != R_NONE)
        {
          Warning("double redirect defs");
        }
        else
        {
          lastRedirect = R_STDERR;
        }
      }
      else if (lastRedirect != R_NONE)
      {
        switch (lastRedirect)
        {
        case R_STDIN:
          cmd->stdin_path = path_expand(sh, item);
          break;
        case R_STDERR:
          cmd->stderr_path = path_expand(sh, item);
          break;
        case R_STDOUT:
          cmd->stdout_path = path_expand(sh, item);
          break;
        default:
          Panic("No this redirect type %d", lastRedirect);
          break;
        }

        lastRedirect = R_NONE;
      }
      else
      {
        if (cmd->head == NULL)
        {
          cmd->head = item;
        }
        else
        {
          Assert(cmd->argc < N_ARGS, "too many args %d", cmd->argc);
          cmd->args[cmd->argc] = item;
          cmd->argc++;
        }
      }
    }
    else
    {
      pmm->free(item);
    }
  }
  cmd->args[cmd->argc] = NULL;
  return 0;
}

void command_free(command *cmd)
{
  pmm->free(cmd->head);
  for (int i = 0; i < cmd->argc; i++)
  {
    pmm->free(cmd->args[i]);
  }
  if (cmd->stderr_path != NULL)
  {
    pmm->free((void *)cmd->stderr_path);
    fsClose(cmd->stderr);
    pmm->free(cmd->stderr);
  }
  if (cmd->stdout_path != NULL)
  {
    pmm->free((void *)cmd->stdout_path);
    fsClose(cmd->stdout);
    pmm->free(cmd->stdout);
  }
  if (cmd->stdin_path != NULL)
  {
    pmm->free((void *)cmd->stdin_path);
    fsClose(cmd->stdin);
    pmm->free(cmd->stdin);
  }
}

int command_openfs(shell_data *sh, command *cmd)
{
  if (cmd->stdin_path == NULL)
    cmd->stdin = sh->stdin;
  else
  {
    filestream *fs = fsOpen(cmd->stdin_path, FILE_READ);
    if (fs == NULL)
    {
      Warning("stdin open failed.");
      fsPrintf(sh->stdout, "Can't open the file %s\n", cmd->stdin_path);
      return -1;
    }
    cmd->stdin = fs;
  }

  if (cmd->stdout_path == NULL)
    cmd->stdout = sh->stdout;
  else
  {
    filestream *fs = fsOpen(cmd->stdout_path, FILE_WRITE);
    if (fs == NULL)
    {
      Warning("stdout open failed.");
      fsPrintf(sh->stdout, "Can't open the file %s\n", cmd->stdout_path);
      return -1;
    }
    cmd->stdout = fs;
  }

  if (cmd->stderr_path == NULL)
    cmd->stderr = sh->stdout;
  else
  {
    filestream *fs = fsOpen(cmd->stderr_path, FILE_WRITE);
    if (fs == NULL)
    {
      Warning("stderr open failed.");
      fsPrintf(sh->stdout, "Can't open the file %s\n", cmd->stderr_path);
      return -1;
    }
    cmd->stderr = fs;
  }
  return 0;
}

static int execute_command(shell_data *sh, command *cmd)
{
  cmd_handler_func handler = NULL;

  {
    int res = vfs->access(cmd->head, FILE_EXIST);
    char *filepath = NULL;

    if (res == -1)
    {
      filepath = path_join("/bin", cmd->head);
      res = vfs->access(filepath, FILE_EXIST);
    }
    else
    {
      filepath = string_create_copy(cmd->head);
    }

    if (res != -1)
    {
      filestream *fs = fsOpen(filepath, FILE_READ);
      char *buf = pmm->alloc(128);
      Assert(fsGetLine(fs, buf, 128) != NULL, "getline failed");
      fsClose(fs);
      int64_t hptr;
      if (atoi(buf, &hptr) == 0)
      {
        handler = (cmd_handler_func)(uintptr_t)hptr;
      }
      else
      {
        fsPrintf(sh->stdout, "The file %s is not executable.", filepath);
      }
    }

    pmm->free(filepath);
  }

  if (handler == NULL)
  {
    fsPrintf(sh->stdout, "Unrecognizable command: %s with %d args\n", cmd->head, cmd->argc);
    return -1;
  }
  else
  {
    command_openfs(sh, cmd);

    if (cmd->stdin == NULL || cmd->stdout == NULL || cmd->stderr == NULL)
    {
      fsPutLine(sh->stdout, "Unable to redirect I/O");
      return -1;
    }

    return handler(sh, cmd);
  }
}

static int parse_execute(shell_data *sh, const char *buf, command *cmd)
{
  // Info("Parse & execute: %s", buf);

  Assert(command_parse(sh, cmd, buf) == 0, "command parse failed.");

  if (cmd->head == NULL)
  {
    return 0;
  }

  int exitCode = execute_command(sh, cmd);

  command_free(cmd);

  fsFlush(sh->stdout);

  return exitCode;
}

#pragma endregion

int cmd_cp(shell_data *sh, command *cmd)
{
  if (cmd->argc != 2)
  {
    fsPutLine(cmd->stderr, "cp needs 2 argument.");
    return -1;
  }
  char *buf = pmm->alloc(128);
  command *subcmd = pmm->alloc(sizeof(command));
  sprintf(buf, "cat < %s > %s", cmd->args[0], cmd->args[1]);
  int res = parse_execute(sh, buf, subcmd);
  pmm->free(subcmd);
  pmm->free(buf);
  return res;
}

int cmd_mv(shell_data *sh, command *cmd)
{
  if (cmd->argc != 2)
  {
    fsPutLine(cmd->stderr, "mv needs 2 argument.");
    return -1;
  }
  char *buf = pmm->alloc(128);
  command *subcmd = pmm->alloc(sizeof(command));
  sprintf(buf, "ln %s %s", cmd->args[0], cmd->args[1]);
  int res = parse_execute(sh, buf, subcmd);
  if (res == 0)
  {
    sprintf(buf, "rm %s", cmd->args[0]);
    res = parse_execute(sh, buf, subcmd);
  }
  pmm->free(subcmd);
  pmm->free(buf);
  return res;
}

int cmd_alias(shell_data *sh, command *cmd)
{
  if (cmd->argc != 2)
  {
    fsPutLine(cmd->stderr, "alias needs 2 argument.");
    return -1;
  }
  char *buf = pmm->alloc(128);
  command *subcmd = pmm->alloc(sizeof(command));
  sprintf(buf, "ln /bin/%s /bin/%s", cmd->args[0], cmd->args[1]);
  int res = parse_execute(sh, buf, subcmd);
  pmm->free(subcmd);
  pmm->free(buf);
  return res;
}

int cmd_unalias(shell_data *sh, command *cmd)
{
  if (cmd->argc != 1)
  {
    fsPutLine(cmd->stderr, "unalias needs 1 argument.");
    return -1;
  }
  char *buf = pmm->alloc(128);
  command *subcmd = pmm->alloc(sizeof(command));
  sprintf(buf, "rm /bin/%s", cmd->args[0]);
  int res = parse_execute(sh, buf, subcmd);
  pmm->free(subcmd);
  pmm->free(buf);
  return res;
}

int cmd_sh(shell_data *sh, command *cmd)
{
  if (cmd->argc != 0)
  {
    fsPutLine(cmd->stderr, "sh needs 0 argument.");
    return -1;
  }
  char *buf = pmm->alloc(128);
  command *subcmd = pmm->alloc(sizeof(command));

  filestream *fs = cmd->stdin;

  int res = 0;

  while (1)
  {
    char *line = fsGetLine(fs, buf, 128);
    if (line == NULL)
      break;
    int tres = parse_execute(sh, buf, subcmd);
    if (tres != 0)
    {
      res = -1;
      fsPrintf(cmd->stderr, "sh: %s failed.", buf);
      break;
    }
  }

  pmm->free(subcmd);
  pmm->free(buf);
  return res;
}

static command_handler command_handlers[N_CMD_HANDLERS] = {
    {
        .head = "hello",
        .handler = cmd_hello,
    },
    {
        .head = "pwd",
        .handler = cmd_pwd,
    },
    {
        .head = "cd",
        .handler = cmd_cd,
    },
    {
        .head = "ls",
        .handler = cmd_ls,
    },
    {
        .head = "cat",
        .handler = cmd_cat,
    },
    {
        .head = "echo",
        .handler = cmd_echo,
    },
    {
        .head = "mkdir",
        .handler = cmd_mkdir,
    },
    {
        .head = "rm",
        .handler = cmd_rm,
    },
    {
        .head = "ln",
        .handler = cmd_ln,
    },
    {
        .head = "cp",
        .handler = cmd_cp,
    },
    {
        .head = "mv",
        .handler = cmd_mv,
    },
    {
        .head = "alias",
        .handler = cmd_alias,
    },
    {
        .head = "unalias",
        .handler = cmd_unalias,
    },
    {
        .head = "sh",
        .handler = cmd_sh,
    },
};

command_handler *get_command_handlers()
{
  return command_handlers;
}

void shell_task(void *_tty_id)
{
  char buf[BUF_CAP];
  int tty_id = (int)_tty_id;

  shell_data *my = pmm->alloc(sizeof(shell_data));
  {
    my->cwd = pmm->alloc(sizeof(buffer));
    buffer_init(my->cwd, 128);
    sprintf(my->cwd->content, "/");
  }

  {
    char *name = pmm->alloc(16);
    sprintf(name, "tty-%d", tty_id);
    my->name = name;
  }

  Info("Start shell %s", my->name);

  sprintf(buf, "/dev/tty%d", tty_id);

  my->stdin = fsOpen(buf, FILE_READ);
  my->stdout = fsOpen(buf, FILE_WRITE);

  command *cmd = pmm->alloc(sizeof(command));

  while (1)
  {
    fsPrintf(my->stdout, "(%s) %s $ ", my->name, my->cwd->content);
    fsFlush(my->stdout);
    Assert(fsGetLine(my->stdin, buf, BUF_CAP) != NULL, "getline failed");

    int exitCode = parse_execute(my, buf, cmd);

    if (exitCode != 0)
    {
      Error("Execute %s with exitcode %d", cmd->head, exitCode);
    }
  }

  pmm->free(cmd);

  fsClose(my->stdin);
  fsClose(my->stdout);
  pmm->free((void *)my->name);
  pmm->free(my);
}