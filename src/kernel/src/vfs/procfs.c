#include <common.h>
#include <klib.h>
#include <debug.h>
#include <tasks.h>
#include <fs.h>
#include <string.h>

// #define FDEV

static const char *FS_NAME = "procfs";
static const char *FS_TYPE = "rdonlyfs-procfs";

enum task_info_type
{
  CPU_INFO,
  MEM_INFO,
  TASK_INFO,
};

typedef struct
{
  int type;
  int hasOpened;
  task_t *obj;
  char *content;
  size_t size;
} task_info;

static int isRoot(inode *node)
{
  return node->ptr == NULL;
}

static int inode_access(file *f, int flags)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a procfs");
  switch (flags)
  {
  case DIR_EXIST:
    return isRoot(f->inode) ? 0 : -1;
  case FILE_EXIST:
  case FILE_READ:
    return isRoot(f->inode) ? -1 : 0;
  case FILE_WRITE:
  case FILE_APPEND:
    Warning("procfs Not support write");
    return -1;
  default:
    Warning("No this flags %d", flags);
    return -1;
  }
  return 0;
}

static int inode_open(file *f, int flags)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a procfs");
  if (inode_access(f, flags) == -1)
    return -1;
  if (flags == DIR_EXIST)
  {
    Warning("DIR_EXIST can't open");
    return -1;
  }
  else
  {
    task_info *tki = (task_info *)f->inode->ptr;
    if (tki->hasOpened == 1)
    {
      Warning("Open an opened file");
      return -1;
    }
    if (tki->type == CPU_INFO)
    {
      tki->content = pmm->alloc(128);
      sprintf(tki->content, "Number of CPU: %d\n", _ncpu());
      tki->size = strlen(tki->content);
    }
    else if (tki->type == MEM_INFO)
    {
      tki->content = pmm->alloc(128);
      sprintf(tki->content, "Memory informention: demo.\n");
      tki->size = strlen(tki->content);
    }
    else if (tki->type == TASK_INFO)
    {
      Assert(tki->obj != NULL, "No task ptr for task_info");
      tki->content = pmm->alloc(128);
      task_t *tk = tki->obj;
      sprintf(tki->content, "Tid: %d\nName: %s\nCPU: %d\n", tk->tid, tk->name, tk->cpu);
      tki->size = strlen(tki->content);
    }
    else
    {
      Panic("Unknown task_info_type %d", tki->type);
    }
    tki->hasOpened = 1;
    f->offset = 0;
  }
  f->flags = flags;
  return 0;
}

static int inode_close(file *f)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a procfs");
  if (isRoot(f->inode))
  {
  }
  else
  {
    task_info *tki = (task_info *)f->inode->ptr;
    if (tki->hasOpened == 0)
    {
      Warning("Close an unopen file");
      return -1;
    }
    pmm->free(tki->content);
    tki->content = NULL;
    tki->size = 0;
    tki->hasOpened = 0;
  }
  return 0;
}

static ssize_t inode_read(file *f, void *buf, size_t size)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a procfs");
  if (f->flags != FILE_READ)
  {
    Warning("Not readable file.");
    return -1;
  }
  else
  {
    task_info *tki = (task_info *)f->inode->ptr;

    if (tki->hasOpened == 0)
    {
      Warning("Read on an unopen file");
      return -1;
    }
    Assert(f->offset <= tki->size, "Offset too large: max %d but %d", tki->size, f->offset);

    size_t rem = tki->size - f->offset;

    strncpy((char *)buf, tki->content + f->offset, rem > size ? size : rem);

    size_t res = strlen(buf);

    f->offset += res;

    return res;
  }
}

static ssize_t inode_write(file *f, const void *buf, size_t size)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a procfs");
  Warning("Unsupportted");
  return -1;
}

// ignore whence, always set
static off_t inode_lseek(file *f, off_t offset, int whence)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a procfs");
  if (f->flags != FILE_READ)
  {
    Warning("Not seekable file.");
    return -1;
  }
  else
  {
    task_info *tki = (task_info *)f->inode->ptr;
    if (tki->hasOpened == 0)
    {
      Warning("Lseek on an unopen file");
      return -1;
    }
    if (offset > tki->size)
    {
      Warning("Lseek too large: max %d, but %d", tki->size, offset);
      offset = tki->size;
    }
    f->offset = offset;
    return f->offset;
  }
}

static int fs_mkdir(filesystem *fs, const char *name)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");
  Warning("Unsupportted");
  return -1;
}

static int fs_rm(filesystem *fs, const char *name)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");
  Warning("Unsupportted");
  return -1;
}

static int fs_link(filesystem *fs, const char *oldpath, const char *newpath)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");
  Warning("Unsupportted");
  return -1;
}

static fslist *fs_readdir(filesystem *fs, const char *path)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");
  if (streq(path, "/"))
  {
    fslist *head = NULL;
    task_node *cur = get_task_list();
    while (cur != NULL)
    {
      fslist *item = pmm->alloc(sizeof(fslist));
      item->name = string_create_copy(cur->task->name);
      item->type = ITEM_FILE;
      item->next = head;
      head = item;
      cur = cur->next;
    }
    {
      fslist *item = pmm->alloc(sizeof(fslist));
      item->name = string_create_copy("cpuinfo");
      item->type = ITEM_FILE;
      item->next = head;
      head = item;
    }
    {
      fslist *item = pmm->alloc(sizeof(fslist));
      item->name = string_create_copy("meminfo");
      item->type = ITEM_FILE;
      item->next = head;
      head = item;
    }
    return head;
  }
  else
  {
    Warning("Not directory.");
    return NULL;
  }
}

static void fs_init(filesystem *fs, const char *name, const char *type, device_t *dev)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");
  Warning("Ignore other args except fs");
}

static inode *fs_lookup(filesystem *fs, const char *path, int flags)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");
  Assert(path[0] == '/', "path %s must start with /", path);

#ifdef FDEV
  Info("Look up on procfs for %s", path);
#endif

  int isroot = strlen(path) == 1;

  inode *node = pmm->alloc(sizeof(inode));
  inodeops *ops = pmm->alloc(sizeof(inodeops));
  inodeinfo *info = pmm->alloc(sizeof(inodeinfo));
  *ops = (inodeops){
      .access = inode_access,
      .open = inode_open,
      .close = inode_close,
      .read = inode_read,
      .write = inode_write,
      .lseek = inode_lseek,
  };

  {
    char *ccpath = pmm->alloc(strlen(path) + 1);
    strcpy(ccpath, path);
    info->path = ccpath;
  }

  *node = (inode){
      .fs = fs,
      .refcnt = 0,
      .ptr = NULL, // ptr is NULL for root
      .ops = ops,
      .info = info,
  };

  if (isroot)
  {
    return node;
  }
  else
  {
    const char *tpath = path + 1;
    if (streq(tpath, "cpuinfo"))
    {
      task_info *tki = pmm->alloc(sizeof(task_info));
      tki->type = CPU_INFO;
      tki->content = NULL;
      tki->size = 0;
      tki->obj = NULL;
      node->ptr = tki;
      return node;
    }
    if (streq(tpath, "meminfo"))
    {
      task_info *tki = pmm->alloc(sizeof(task_info));
      tki->type = MEM_INFO;
      tki->content = NULL;
      tki->size = 0;
      tki->obj = NULL;
      tki->hasOpened = 0;
      node->ptr = tki;
      return node;
    }
    else
    {
      task_t *tk = task_lookup(path + 1);
      if (tk == NULL)
      {
        Warning("task %s not found", path + 1);
        inode_free(node);
        return NULL;
      }
      else
      {
        task_info *tki = pmm->alloc(sizeof(task_info));
        tki->type = TASK_INFO;
        tki->content = NULL;
        tki->size = 0;
        tki->obj = tk;
        tki->hasOpened = 0;
        node->ptr = tki;
        return node;
      }
    }
  }
}

static int fs_close(filesystem *fs, inode *inode)
{
  Assert(streq(fs->name, FS_NAME), "Not a procfs");

#ifdef FDEV
  Info("Close %s on devfs", inode->info->path);
#endif

  if (inode == NULL)
  {
    Warning("Close a NULL inode");
    return -1;
  }
  if (inode->ptr != NULL)
  {
    task_info *tki = inode->ptr;
    if (tki->hasOpened)
    {
      pmm->free(tki->content);
    }
    pmm->free(tki);
  }
  inode_free(inode);
  return 0;
}

filesystem *procfs_create()
{
  filesystem *fs = pmm->alloc(sizeof(filesystem));
  fsops *ops = pmm->alloc(sizeof(fsops));
  *ops = (fsops){
      .init = fs_init,
      .lookup = fs_lookup,
      .close = fs_close,
      .mkdir = fs_mkdir,
      .rm = fs_rm,
      .link = fs_link,
      .readdir = fs_readdir,
  };
  *fs = (filesystem){
      .name = FS_NAME,
      .type = FS_TYPE,
      .ops = ops,
      .dev = NULL,
      .ptr = NULL,
  };
  return fs;
}