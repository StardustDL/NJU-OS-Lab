#ifndef __COMMON_H__
#define __COMMON_H__

#include <kernel.h>
#include <nanos.h>
#include <stdint.h>

#define IS_ARCH_X86 (defined(__X86_H__))

typedef uint8_t bool;

#define true 1
#define false 0

typedef struct _inode inode;
typedef struct _filesystem filesystem;
typedef struct _inodeops inodeops;
typedef struct _fsops fsops;
typedef struct _file file;
typedef struct _inodeinfo inodeinfo;
typedef struct _fslist fslist;

#define N_FILE 16

typedef enum _task_state
{
  Free,
  Working
} task_state;

struct task
{
  int tid;
  const char *name;
  void (*entry)(void *);
  void *arg;
  _Context context;
  uint8_t *stack;
  int cpu;
  task_state state;
  file *fildes[N_FILE];
};

struct spinlock
{
  const char *name;
  volatile intptr_t locked;
  int cpu;
};

struct semaphore
{
  const char *name;
  spinlock_t lock;
  int value;
};

typedef struct _buffer
{
  char *content;
  size_t buf_cap;
  size_t buf_size;
  size_t buf_pos;
} buffer;

struct _file
{
  int refcnt;
  int flags;
  inode *inode;
  uint64_t offset;
};

typedef struct _filestream
{
  int fd;
  int enable_line_buf;
  buffer *buf;
} filestream;

enum fslist_type{
  ITEM_DIR,
  ITEM_FILE,
  ITEM_LINK,
};

struct _fslist
{
  const char *name;
  int type;
  struct _fslist *next;
};

struct _inodeops
{
  int (*access)(file *f, int flags);
  int (*open)(file *f, int flags);
  int (*close)(file *f);
  ssize_t (*read)(file *f, void *buf, size_t size);
  ssize_t (*write)(file *f, const void *buf, size_t size);
  off_t (*lseek)(file *f, off_t offset, int whence);
};

struct _inode
{
  int refcnt;
  void *ptr;
  filesystem *fs;
  inodeops *ops;
  inodeinfo *info;
};

struct _inodeinfo
{
  const char *path;
};

struct _fsops
{
  void (*init)(filesystem *fs, const char *name, const char *type, device_t *dev);
  inode *(*lookup)(filesystem *fs, const char *path, int flags);
  int (*close)(filesystem *fs, inode *inode);
  int (*mkdir)(filesystem *fs, const char *path);
  int (*rm)(filesystem *fs, const char *path);
  int (*link)(filesystem *fs, const char *oldpath, const char *newpath);
  fslist *(*readdir)(filesystem *fs, const char *path);
};

struct _filesystem
{
  const char *name;
  const char *type;
  void *ptr;
  fsops *ops;
  device_t *dev;
};

typedef struct
{
  void (*init)();
  int (*access)(const char *path, int flags);
  int (*mount)(const char *path, filesystem *fs);
  int (*unmount)(const char *path);
  int (*mkdir)(const char *path);
  int (*rm)(const char *path);
  int (*link)(const char *oldpath, const char *newpath);
  fslist *(*readdir)(const char *path);
  int (*open)(const char *path, int flags);
  ssize_t (*read)(int fd, void *buf, size_t nbyte);
  ssize_t (*write)(int fd, void *buf, size_t nbyte);
  off_t (*lseek)(int fd, off_t offset, int whence);
  int (*close)(int fd);
} MODULE(vfs);

#endif
