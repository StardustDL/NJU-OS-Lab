#pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wreturn-type"
// #pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <common.h>
#include <klib.h>
#include <debug.h>
#include <fs.h>
#include <devices.h>
#include <tasks.h>
#include <buffer.h>
#include <path.h>
#include <shell.h>

// #define MDEV
#define EN_LOCK

#pragma region mount list

typedef struct _mount_node
{
  char *path;
  filesystem *fs;
  struct _mount_node *next;
} mount_node;

static mount_node *mountlist_head = NULL;

static spinlock_t mount_list_lock;

static mount_node *mount_list_find(const char *path, mount_node **last)
{
  if (path == NULL)
    return NULL;

  kmt->spin_lock(&mount_list_lock);

  mount_node *cur = mountlist_head;
  *last = NULL;
  int finded = 0;
  while (cur != NULL)
  {
    if (streq(path, cur->path))
    {
      finded = 1;
      break;
    }
    *last = cur;
    cur = cur->next;
  }

  kmt->spin_unlock(&mount_list_lock);

  if (finded)
  {
    return cur;
  }
  else
  {
    *last = NULL;
    return NULL;
  }
}

// return head
static fslist *mount_list_readdir(const char *path, fslist **tail)
{
  Assert(path != NULL, "path is NULL");

  kmt->spin_lock(&mount_list_lock);

  fslist *head = NULL;
  *tail = NULL;

  char *target_dir = NULL;
  {
    int len = strlen(path);
    target_dir = pmm->alloc(len + 2);
    if (path[len - 1] == '/')
    {
      sprintf(target_dir, "%s", path);
    }
    else
    {
      sprintf(target_dir, "%s/", path);
    }
  }

  mount_node *cur = mountlist_head;
  while (cur != NULL)
  {
    char *dir = path_get_directory(cur->path);
    if (streq(target_dir, dir))
    {
      char *name = path_get_name(cur->path);
      if (strlen(name) == 0) // ignore self match
      {
        pmm->free(name);
      }
      else
      {
        fslist *node = pmm->alloc(sizeof(fslist));
        node->name = path_get_name(cur->path);
        node->type = ITEM_DIR;
        node->next = head;
        if (head == NULL)
          *tail = node;
        head = node;
      }
    }
    pmm->free(dir);
    cur = cur->next;
  }

  pmm->free(target_dir);

  kmt->spin_unlock(&mount_list_lock);

  return head;
}

static void mount_list_append(const char *path, filesystem *fs)
{
  Assert(path != NULL, "append mount.path is NULL");
  Assert(fs != NULL, "append mount.fs is NULL");
  {
    mount_node *cur, *last = NULL;
    cur = mount_list_find(path, &last);
    Assert(cur == NULL, "The mount path %s exists.", path);
  }
  mount_node *fn = pmm->alloc(sizeof(mount_node));

  fn->path = pmm->alloc(strlen(path) + 1);
  Assert(fn->path != NULL, "path alloc failed");
  strcpy(fn->path, path);

  fn->fs = fs;

  kmt->spin_lock(&mount_list_lock);

  fn->next = mountlist_head;
  mountlist_head = fn;

  kmt->spin_unlock(&mount_list_lock);
}

static void mount_list_remove(const char *path)
{
  if (path == NULL)
    return;
  mount_node *cur, *last = NULL;
  cur = mount_list_find(path, &last);
  if (cur == NULL)
  {
    Warning("Not find the mount path in list.");
  }
  else
  {
    kmt->spin_lock(&mount_list_lock);

    if (last == NULL)
    {
      Assert(cur == mountlist_head, "cur needs to be equal to head");
      mountlist_head = cur->next;
    }
    else
    {
      last->next = cur->next;
    }

    kmt->spin_unlock(&mount_list_lock);

    pmm->free(cur->path);
    pmm->free(cur);
  }
}

#pragma endregion

#pragma region file list

typedef struct _file_node
{
  file *file;
  struct _file_node *next;
} file_node;

static file_node *filelist_head = NULL;

static spinlock_t file_list_lock;

static void file_list_append(file *file)
{
  Assert(file != NULL, "append file is NULL");
  file_node *fn = pmm->alloc(sizeof(file_node));
  fn->file = file;

  kmt->spin_lock(&file_list_lock);
  fn->next = filelist_head;
  filelist_head = fn;
  kmt->spin_unlock(&file_list_lock);
}

static void file_list_remove(file *file)
{
  if (file == NULL)
    return;
  file_node *cur = filelist_head, *last = NULL;
  int finded = 0;
  while (cur != NULL)
  {
    if (cur->file == file)
    {
      finded = 1;
      break;
    }
    last = cur;
    cur = cur->next;
  }
  if (!finded)
  {
    Warning("Not find the file in list.");
  }
  else
  {
    kmt->spin_lock(&file_list_lock);
    if (last == NULL)
    {
      Assert(cur == filelist_head, "cur needs to be equal to head");
      filelist_head = cur->next;
    }
    else
    {
      last->next = cur->next;
    }
    kmt->spin_unlock(&file_list_lock);
    pmm->free(cur);
  }
}

#pragma endregion

static spinlock_t vfs_lock;

static int find_filde_index(task_t *tk, file *f)
{
  int res = -1;
  for (int i = 0; i < N_FILE; i++)
  {
    if (tk->fildes[i] == f)
    {
      res = i;
      break;
    }
  }
  return res;
}

static file *get_file(int fd)
{
  file *tf = current_task()->fildes[fd];
  if (tf == NULL)
  {
    Warning("Empty fd %d", fd);
  }
  return tf;
}

static mount_node *get_mount_fs(const char *path, const char **subpath)
{
  Assert(path != NULL, "query path is NULL");

  kmt->spin_lock(&mount_list_lock);

  mount_node *cur = mountlist_head;
  mount_node *res = NULL;
  int bestLen = -1;
  while (cur != NULL)
  {
    int len = strlen(cur->path);
    if (strneq(path, cur->path, len))
    {
      if (len > bestLen)
      {
        bestLen = len;
        res = cur;
      }
    }
    cur = cur->next;
  }

  *subpath = (const char *)((uintptr_t)path + bestLen);
  if (**subpath == '\0') // path like /dev
    *subpath = "/";
  else if (**subpath != '/') // path like /bin
  {
    (*subpath)--;
  }

  kmt->spin_unlock(&mount_list_lock);

  return res;
}

static int access(const char *path, int flags)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Access %s for %s", path, current_task()->name);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  const char *subpath = NULL;

  mount_node *mn = get_mount_fs(path, &subpath);
  if (mn == NULL)
  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
    Warning("Not found %s from mount list", path);
#endif

    return -1;
  }

  // Info("Find mount fs %s on %s", mn->fs->name, mn->path);

  Assert(mn->fs != NULL, "No fs for %s", mn->path);

  inode *node = NULL;
  {
    filesystem *fs = mn->fs;

    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif

      node = fs->ops->lookup(fs, subpath, flags);

#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }

    if (node == NULL)
    {
      Warning("Not found %s from fs %s on mount path %s", subpath, fs->name, mn->path);
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif
      return -1;
    }
  }

  filesystem *fs = node->fs;
  file *tf = pmm->alloc(sizeof(file));

  Assert(tf != NULL, "alloc failed");

  tf->inode = node;

  int res = -1;

  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif

    res = node->ops->access(tf, flags);

    Assert(fs->ops->close(fs, node) == 0, "FS close inode failed");

#ifdef EN_LOCK
    kmt->spin_lock(&vfs_lock);
#endif
  }

  pmm->free(tf);

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Accessed %s for %s", path, current_task()->name);
#endif

  return res;
}

static int mount(const char *path, filesystem *fs)
{
  Assert(fs != NULL, "Mount fs is NULL");

#ifdef MDEV
  Info("Mount %s(%s) on %s", fs->name, fs->type, path);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  mount_node *cur, *last;
  cur = mount_list_find(path, &last);
  int res = -1;
  if (cur == NULL)
  {
    mount_list_append(path, fs);
    res = 0;
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Monted %s(%s) on %s", fs->name, fs->type, path);
#endif
  return res;
}

static int unmount(const char *path)
{
#ifdef MDEV
  Info("Unmount on %s", path);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  mount_list_remove(path);

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Unmount on %s", path);
#endif
  return 0;
}

static int mkdir(const char *path)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Mkdir %s for %s", path, current_task()->name);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  const char *subpath = NULL;

  mount_node *mn = get_mount_fs(path, &subpath);
  if (mn == NULL)
  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif
    Warning("Not found %s from mount list", path);
    return -1;
  }

  // Info("Find mount fs %s on %s", mn->fs->name, mn->path);

  Assert(mn->fs != NULL, "No fs for %s", mn->path);

  int res = -1;
  {
    filesystem *fs = mn->fs;

    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif

      res = fs->ops->mkdir(fs, subpath);

#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Mkdir-ed %s for %s", path, current_task()->name);
#endif
  return res;
}

static int rm(const char *path)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Rmdir %s for %s", path, current_task()->name);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  const char *subpath = NULL;

  mount_node *mn = get_mount_fs(path, &subpath);
  if (mn == NULL)
  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif
    Warning("Not found %s from mount list", path);
    return -1;
  }

  // Info("Find mount fs %s on %s", mn->fs->name, mn->path);

  Assert(mn->fs != NULL, "No fs for %s", mn->path);

  int res = -1;
  {
    filesystem *fs = mn->fs;

    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif

      res = fs->ops->rm(fs, subpath);

#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Rmdir-ed %s for %s", path, current_task()->name);
#endif
  return res;
}

static int link(const char *oldpath, const char *newpath)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Link from %s to %s for %s", oldpath, newpath, current_task()->name);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  const char *oldSubpath = NULL;
  const char *newSubpath = NULL;

  mount_node *mn = get_mount_fs(oldpath, &oldSubpath);
  {
    mount_node *tmn = get_mount_fs(newpath, &newSubpath);
    if (mn != tmn)
    {
      Warning("The two path is at different mount point: %s and %s", mn->path, tmn->path);
      return -1;
    }
  }
  if (mn == NULL)
  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif
    Warning("Not found %s from mount list", oldpath);
    return -1;
  }

  // Info("Find mount fs %s on %s", mn->fs->name, mn->path);

  Assert(mn->fs != NULL, "No fs for %s", mn->path);

  int res = -1;
  {
    filesystem *fs = mn->fs;

    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif

      res = fs->ops->link(fs, oldSubpath, newSubpath);

#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Linked from %s to %s for %s", oldpath, newpath, current_task()->name);
#endif
  return res;
}

static int open(const char *path, int flags)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Open %s", path);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  int empty_fd = find_filde_index(current_task(), NULL);
  if (empty_fd == -1)
  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif
    Warning("No empty fd for task %s", current_task()->name);
    return -1;
  }

  const char *subpath = NULL;

  mount_node *mn = get_mount_fs(path, &subpath);
  if (mn == NULL)
  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif
    Warning("Not found %s from mount list", path);
    return -1;
  }

  // Info("Find mount fs %s on %s", mn->fs->name, mn->path);

  Assert(mn->fs != NULL, "No fs for %s", mn->path);

  inode *node = NULL;
  {
    filesystem *fs = mn->fs;

    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif

      node = fs->ops->lookup(fs, subpath, flags);

#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }

    if (node == NULL)
    {
      Warning("Not found %s from fs %s on mount path %s", subpath, fs->name, mn->path);
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif
      return -1;
    }
  }

  filesystem *fs = node->fs;
  file *tf = pmm->alloc(sizeof(file));

  Assert(tf != NULL, "alloc failed");

  tf->inode = node;

  int res = -1;

  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif

    res = node->ops->open(tf, flags);

#ifdef EN_LOCK
    kmt->spin_lock(&vfs_lock);
#endif
  }

  tf->refcnt = 0;

  if (res == 0)
  {
    file_list_append(tf);

    current_task()->fildes[empty_fd] = tf;
  }
  else
  {
    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif

      Assert(fs->ops->close(fs, node) == 0, "FS close inode failed");

#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }

    pmm->free(tf);
    Warning("Open with non-zero %d", res);
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef EN_LOCK
// kmt->spin_unlock(&vfs_lock);
#endif
#ifdef MDEV
  Info("Opened file %s for %s at fd %d", path, current_task()->name, empty_fd);
#endif

  return res == 0 ? empty_fd : -1;
}

static ssize_t read(int fd, void *buf, size_t nbyte)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Read most %d bytes for %s at fd %d", nbyte, current_task()->name, fd);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  file *tf = get_file(fd);
  Assert(tf != NULL, "fd %d not found", fd);

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

  size_t res = tf->inode->ops->read(tf, buf, nbyte);

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Readed %d bytes for %s at fd %d", res, current_task()->name, fd);
#endif

  return res;
}

static ssize_t write(int fd, void *buf, size_t nbyte)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Write %d bytes for %s at fd %d", nbyte, current_task()->name, fd);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  file *tf = get_file(fd);
  Assert(tf != NULL, "fd %d not found", fd);

  ssize_t res = -1;

  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif

    res = tf->inode->ops->write(tf, buf, nbyte);

#ifdef EN_LOCK
    kmt->spin_lock(&vfs_lock);
#endif
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Writed %d bytes for %s at fd %d", res, current_task()->name, fd);
#endif
  return res;
}

static off_t lseek(int fd, off_t offset, int whence)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Lseek to %d for %s at fd %d", offset, current_task()->name, fd);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  file *tf = get_file(fd);
  Assert(tf != NULL, "fd %d not found", fd);
  off_t res = -1;

  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif

    res = tf->inode->ops->lseek(tf, offset, whence);

#ifdef EN_LOCK
    kmt->spin_lock(&vfs_lock);
#endif
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Lseeked to %d for %s at fd %d", offset, current_task()->name, fd);
#endif
  return res;
}

static fslist *readdir(const char *path)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Readdir for %s", current_task()->name);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  fslist *tail = NULL;
  fslist *res = mount_list_readdir(path, &tail);

  const char *subpath = NULL;

  mount_node *mn = get_mount_fs(path, &subpath);
  if (mn == NULL)
  {
    Warning("Not found %s from mount list", path);
  }
  else
  {
    Assert(mn->fs != NULL, "No fs for %s", mn->path);

    {
      filesystem *fs = mn->fs;

      {
#ifdef EN_LOCK
        kmt->spin_unlock(&vfs_lock);
#endif

        fslist *subfs = fs->ops->readdir(fs, subpath);
        if (res == NULL)
          res = subfs;
        else
        {
          Assert(tail != NULL, "tail is NULL");
          tail->next = subfs;
        }

        // TODO: Search in mount list

#ifdef EN_LOCK
        kmt->spin_lock(&vfs_lock);
#endif
      }
    }
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Readdir-ed for %s", current_task()->name);
#endif
  return res;
}

// TODO: Not share fd now
static int close(int fd)
{
  Assert(current_task() != NULL, "current task is null");

#ifdef MDEV
  Info("Close for %s at fd %d", current_task()->name, fd);
#endif

#ifdef EN_LOCK
  kmt->spin_lock(&vfs_lock);
#endif

  file *tf = get_file(fd);
  Assert(tf != NULL, "fd %d not found", fd);

  filesystem *fs = tf->inode->fs;
  int res = -1;

  {
#ifdef EN_LOCK
    kmt->spin_unlock(&vfs_lock);
#endif

    res = tf->inode->ops->close(tf);

#ifdef EN_LOCK
    kmt->spin_lock(&vfs_lock);
#endif
  }

  if (res == 0)
  {
    {
#ifdef EN_LOCK
      kmt->spin_unlock(&vfs_lock);
#endif
      Assert(fs->ops->close(fs, tf->inode) == 0, "FS close inode failed");
#ifdef EN_LOCK
      kmt->spin_lock(&vfs_lock);
#endif
    }

    file_list_remove(tf);
    current_task()->fildes[fd] = NULL;
  }
  else
  {
    Warning("Close with non-zero %d", res);
  }

#ifdef EN_LOCK
  kmt->spin_unlock(&vfs_lock);
#endif

#ifdef MDEV
  Info("Closed for %s at fd %d", current_task()->name, fd);
#endif
  return res;
}

static void initrd_write(filesystem *fs, const char *path, const void *buf, size_t count)
{
  inode *node = fs->ops->lookup(fs, path, FILE_WRITE);
  Assert(node != NULL, "failed");
  file *f = pmm->alloc(sizeof(file));
  f->refcnt = 0;
  f->inode = node;
  Assert(node->ops->open(f, FILE_WRITE) == 0, "failed");
  Assert(node->ops->write(f, buf, count) == count, "failed");
  Assert(node->ops->close(f) == 0, "failed");
  Assert(fs->ops->close(fs, node) == 0, "failed");
  pmm->free(f);
}

static void init()
{
#ifdef MDEV
  InfoN("Module VFS initializing");
#endif

  filelist_head = NULL;
  mountlist_head = NULL;
  kmt->spin_init(&vfs_lock, "vfs-lock");
  kmt->spin_init(&mount_list_lock, "vfs-mount-list-lock");
  kmt->spin_init(&file_list_lock, "vfs-file-list-lock");

  {
    filesystem *devfs = devfs_create();
    devfs->ops->init(devfs, "", "", NULL);
    mount("/dev", devfs);
  }

  {
    filesystem *procfs = procfs_create();
    procfs->ops->init(procfs, "", "", NULL);
    mount("/proc", procfs);
  }

  {
    {
      device_t *dev = dev_lookup("ramdisk0");
      Assert(dev != NULL, "device ramdisk0 not found");
      filesystem *blkfs = blkfs_create();
      blkfs->ops->init(blkfs, "blkfs-ramdisk0", NULL, dev);

      Assert(blkfs->ops->mkdir(blkfs, "/bin") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/tmp") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/etc") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/boot") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/opt") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/usr") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/sys") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/var") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/home") == 0, "failed");
      Assert(blkfs->ops->mkdir(blkfs, "/home/my") == 0, "failed");

      {
        const char *hello_text = "Hello world!\n";
        initrd_write(blkfs, "/home/my/hello.txt", hello_text, strlen(hello_text));
      }

      {
        const char *sh_text = "echo all_commands_(under_/bin/)\nls /bin\necho all_procs_(under_/proc/)\nls /proc\necho all_devices_(under_/dev/)\nls /dev";
        initrd_write(blkfs, "/home/my/stats.sh", sh_text, strlen(sh_text));
      }

      {
        char *path = pmm->alloc(128);
        char *buf = pmm->alloc(128);
        command_handler *command_handlers = get_command_handlers();
        for (int i = 0; i < N_CMD_HANDLERS; i++)
        {
          command_handler *cmd = command_handlers + i;
          Assert(cmd->head != NULL, "cmd head is NULL");
          sprintf(path, "/bin/%s", cmd->head);
          sprintf(buf, "%u", (uintptr_t)cmd->handler);
          initrd_write(blkfs, path, buf, strlen(buf));

#ifdef MDEV
          Info("Registerd command %s at %s", path, buf);
#endif
        }
        pmm->free(path);
        pmm->free(buf);
      }

      mount("/", blkfs);
    }

    {
      device_t *dev = dev_lookup("ramdisk1");
      Assert(dev != NULL, "device ramdisk1 not found");
      filesystem *blkfs = blkfs_create();
      blkfs->ops->init(blkfs, "blkfs-ramdisk1", NULL, dev);
      mount("/mnt", blkfs);
    }
  }

#ifdef MDEV
  PassN("Module VFS initialized");
#endif
}

MODULE_DEF(vfs){
    .init = init,
    .access = access,
    .mount = mount,
    .unmount = unmount,
    .mkdir = mkdir,
    .rm = rm,
    .link = link,
    .open = open,
    .read = read,
    .write = write,
    .lseek = lseek,
    .close = close,
    .readdir = readdir,
};