#include <common.h>
#include <klib.h>
#include <debug.h>
#include <devices.h>
#include <fs.h>
#include <string.h>

// #define FDEV

#define DEV_CNT 8

static const char *FS_NAME = "devfs";
static const char *FS_TYPE = "rwfs-devfs";

static char *dev_names[] = {
    "ramdisk0",
    "ramdisk1",
    "input",
    "fb",
    "tty1",
    "tty2",
    "tty3",
    "tty4",
};

static int isRoot(inode *node)
{
  return node->ptr == NULL;
}

static int inode_access(file *f, int flags)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a devfs");
  switch (flags)
  {
  case DIR_EXIST:
    if (isRoot(f->inode))
    {
      f->flags = flags;
      return 0;
    }
    else
    {
      return -1;
    }
  case FILE_EXIST:
  case FILE_READ:
  case FILE_WRITE:
  case FILE_APPEND:
    if (!isRoot(f->inode))
    {
      f->flags = flags;
      return 0;
    }
    else
    {
      return -1;
    }
  default:
    Warning("No this flags %d", flags);
    return -1;
  }
  return 0;
}

static int inode_open(file *f, int flags)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a devfs");
  if (inode_access(f, flags) == -1)
    return -1;  
  switch (flags)
  {
  case DIR_EXIST:
    Warning("DIR_EXIST can't open");
    return -1;
  case FILE_READ:
    f->offset = 0;
    f->flags = flags;
    break;
  case FILE_WRITE:
    f->offset = 0;
    f->flags = flags;
    break;
  case FILE_APPEND:
    Warning("devfs not support FILE_APPEND flags");
    f->offset = 0;
    f->flags = flags;
    f->flags = FILE_WRITE;
    break;
  }
  return 0;
}

static int inode_close(file *f)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a devfs");
  return 0;
}

static ssize_t inode_read(file *f, void *buf, size_t size)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a devfs");
  if (f->flags != FILE_READ)
  {
    Warning("Not readable file.");
    return -1;
  }
  else
  {
    device_t *dev = (device_t *)f->inode->ptr;
    ssize_t res = dev->ops->read(dev, f->offset, buf, size);
    f->offset += res;
    return res;
  }
}

static ssize_t inode_write(file *f, const void *buf, size_t size)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a devfs");
  if (f->flags != FILE_WRITE)
  {
    Warning("Not readable file.");
    return -1;
  }
  else
  {
    device_t *dev = (device_t *)f->inode->ptr;
    ssize_t res = dev->ops->write(dev, f->offset, buf, size);
    f->offset += res;
    return res;
  }
}

// ignore whence, always set
static off_t inode_lseek(file *f, off_t offset, int whence)
{
  Assert(streq(f->inode->fs->name, FS_NAME), "Not a devfs");
  if (f->flags != FILE_WRITE && f->flags != FILE_READ)
  {
    Warning("Not seekable file.");
    return -1;
  }
  else
  {
    f->offset = offset;
    return f->offset;
  }
}

static int fs_mkdir(filesystem *fs, const char *name)
{
  Assert(streq(fs->name, FS_NAME), "Not a devfs");
  Warning("Unsupportted");
  return -1;
}

static int fs_rm(filesystem *fs, const char *name)
{
  Assert(streq(fs->name, FS_NAME), "Not a devfs");
  Warning("Unsupportted");
  return -1;
}

static int fs_link(filesystem *fs, const char *oldpath, const char *newpath)
{
  Assert(streq(fs->name, FS_NAME), "Not a devfs");
  Warning("Unsupportted");
  return -1;
}

static fslist *fs_readdir(filesystem *fs, const char *path)
{
  Assert(streq(fs->name, FS_NAME), "Not a devfs");
  if (streq(path, "/"))
  {
    fslist *head = NULL;
    for (int i = 0; i < DEV_CNT; i++)
    {
      fslist *item = pmm->alloc(sizeof(fslist));
      item->name = string_create_copy(dev_names[i]);
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
  Assert(streq(fs->name, FS_NAME), "Not a devfs");
  Warning("Ignore other args except fs");
}

static inode *fs_lookup(filesystem *fs, const char *path, int flags)
{
  Assert(streq(fs->name, FS_NAME), "Not a devfs");
  Assert(path[0] == '/', "path %s must start with /", path);

#ifdef FDEV
  Info("Look up on devfs for %s", path);
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
    device_t *dev = dev_lookup(path + 1);
    if (dev == NULL)
    {
      Warning("device %s not found", path + 1);
      inode_free(node);
      return NULL;
    }
    else
    {
      node->ptr = dev; // ptr save pointer to device
      return node;
    }
  }
}

static int fs_close(filesystem *fs, inode *inode)
{
  Assert(streq(fs->name, FS_NAME), "Not a devfs");

#ifdef FDEV
  Info("Close %s on devfs", inode->info->path);
#endif

  if (inode == NULL)
  {
    Warning("Close a NULL inode");
    return -1;
  }
  inode_free(inode);
  return 0;
}

filesystem *devfs_create()
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