#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-function"

#include <common.h>
#include <klib.h>
#include <string.h>
#include <debug.h>
#include <devices.h>
#include <path.h>
#include <fs.h>

// #define FDEV

static const char *FS_NAME = "blkfs";
static const char *FS_TYPE = "rwfs-blkfs";

#define INODE_NAME_LEN 16

const int BLOCK_DATA_SIZE = 1024;

#pragma region data structure

const int MAGIC = 54987631;

typedef struct _blkfs_info
{
  uint32_t full_size;
  uint32_t block_size;
  uint32_t block_data_size;
  uint32_t inode_block_num;
  uint32_t inode_per_block;
  uint32_t child_per_dir;
  uint32_t total_block_num;
  device_t *dev;
  spinlock_t lock;
} blkfs_info;

enum blkfs_block_type
{
  FREE,
  USING,
};

typedef struct _blkfs_block
{
  int magic;
  int type;
} blkfs_block;

static void block_read(char *buf, size_t count, uintptr_t block_id, size_t offset, blkfs_info *fs)
{
#ifdef FDEV
  // Info("Read %u bytes in block %u at offset %u on %s", count, block_id, offset, fs->dev->name);
#endif

  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);
  Assert(offset + count <= fs->block_size, "offset + count %u is too large for block_size %u", offset + count, fs->block_size);
  if (count == 0)
    return;
  uint64_t pos = block_id * fs->block_size + offset;
  ssize_t readed = fs->dev->ops->read(fs->dev, pos, buf, count);
  Assert(readed == count, "Readed %u but needs %u", readed, count);

#ifdef FDEV
  // Info("Readed most %u bytes in block %u at offset %u on %s", count, block_id, offset, fs->dev->name);
#endif
}

static void block_write(const char *buf, size_t count, uintptr_t block_id, size_t offset, blkfs_info *fs)
{
#ifdef FDEV
  // Info("Write %u bytes in block %u at offset %u on %s", count, block_id, offset, fs->dev->name);
#endif

  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);
  Assert(offset + count <= fs->block_size, "offset + count %u is too large for block_size %u", offset + count, fs->block_size);
  if (count == 0)
    return;
  uint64_t pos = block_id * fs->block_size + offset;
  ssize_t writen = fs->dev->ops->write(fs->dev, pos, buf, count);
  Assert(writen == count, "Writen %u but needs %u", writen, count);

#ifdef FDEV
  // Info("Writen %u bytes in block %u at offset %u on %s", count, block_id, offset, fs->dev->name);
#endif
}

// return 0 if pass
static int block_check(uintptr_t block_id, blkfs_info *fs)
{
  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);

  blkfs_block *blk = pmm->alloc(sizeof(blkfs_block));

  Assert(blk != NULL, "alloc blk failed");

  block_read((char *)blk, sizeof(blkfs_block), block_id, 0, fs);

  int res = blk->magic == MAGIC ? 0 : 1;

  pmm->free(blk);

#ifdef FDEV
  if (res != 0)
  {
    Warning("Block %u check failed on %s", block_id, fs->dev->name);
  }
#endif

  return res;
}

static blkfs_block *block_getinfo(uintptr_t block_id, blkfs_info *fs)
{
  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);

  blkfs_block *blk = pmm->alloc(sizeof(blkfs_block));

  block_read((char *)blk, sizeof(blkfs_block), block_id, 0, fs);

  Assert(blk->magic == MAGIC, "block %u magic check failed.", block_id);

  return blk;
}

static void block_setinfo(blkfs_block *blk, uintptr_t block_id, blkfs_info *fs)
{
  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);
  Assert(blk->magic == MAGIC, "block magic check failed.", block_id);

  block_write((char *)blk, sizeof(blkfs_block), block_id, 0, fs);
}

static ssize_t block_readdata(char *buf, size_t count, uintptr_t block_id, size_t offset, blkfs_info *fs)
{
  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);
  Assert(offset + count <= fs->block_data_size, "count %u is too large for block_data_size %u", count, fs->block_data_size);

  block_read(buf, count, block_id, sizeof(blkfs_block) + offset, fs);

  return count;
}

static ssize_t block_writedata(const char *buf, size_t count, uintptr_t block_id, size_t offset, blkfs_info *fs)
{
  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);
  Assert(offset + count <= fs->block_data_size, "count %u is too large for block_data_size %u", count, fs->block_data_size);

  block_write(buf, count, block_id, offset + sizeof(blkfs_block), fs);

  return count;
}

static void block_init(uintptr_t block_id, blkfs_info *fs)
{
  Assert(fs != NULL, "No fs.");
  Assert(block_id < fs->total_block_num, "block_id %u is too large, needs to smaller than %u", block_id, fs->total_block_num);

  blkfs_block *blk = pmm->alloc(sizeof(blkfs_block));
  blk->magic = MAGIC;
  blk->type = FREE;

  block_setinfo(blk, block_id, fs);

  pmm->free(blk);
}

// return id and if no free block, return -1
static int block_alloc(blkfs_info *fs)
{
#ifdef FDEV
  Info("Alloc block on %s", fs->dev->name);
#endif

  Assert(fs != NULL, "No fs.");
  int res = -1;
  for (int i = fs->inode_block_num; i < fs->total_block_num; i++)
  {
    blkfs_block *blk = block_getinfo(i, fs);
    if (blk->type == FREE)
    {
      blk->type = USING;
      res = i;
      block_setinfo(blk, i, fs);
      pmm->free(blk);
      break;
    }
    else
    {
      pmm->free(blk);
    }
  }

#ifdef FDEV
  Info("Alloced block %d on %s", res, fs->dev->name);
#endif
  return res;
}

static void block_free(uintptr_t block_id, blkfs_info *fs)
{
#ifdef FDEV
  Info("Free block %u on %s", block_id, fs->dev->name);
#endif

  Assert(fs != NULL, "No fs.");
  blkfs_block *blk = block_getinfo(block_id, fs);
  Assert(blk->type == USING, "block %u is not using.", block_id);
  blk->type = FREE;
  block_setinfo(blk, block_id, fs);
  pmm->free(blk);

#ifdef FDEV
  Info("Free-ed block %u on %s", block_id, fs->dev->name);
#endif
}

#pragma endregion

#pragma region core function

enum blkfs_inode_type
{
  EMPTY,
  FILE,
  DIR,
  LINK,
};

typedef struct _blkfs_inode
{
  int type;
  int id;
  int block_id;
  int refcnt;
  uint32_t size;
} blkfs_inode;

typedef struct
{
  int type; // FREE or USING
  char name[INODE_NAME_LEN];
  int inode_id;
} dir_entry;

static blkfs_inode *inode_get(uint32_t id, blkfs_info *fs)
{
#ifdef FDEV
  Info("Get inode %u on %s", id, fs->dev->name);
#endif

  int blkid = id / fs->inode_per_block;
  int tid = id % fs->inode_per_block;
  blkfs_inode *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, blkid, 0, fs) == fs->block_data_size, "can't read enough");
  blkfs_inode *res = NULL;
  if (items[tid].type == EMPTY)
  {
    Warning("The id %u of inode gets empty.", id);
  }
  else
  {
    res = pmm->alloc(sizeof(blkfs_inode));
    memcpy(res, &items[tid], sizeof(blkfs_inode));
  }
  pmm->free(items);

#ifdef FDEV
  Info("Geted inode %u on %s", id, fs->dev->name);
#endif

  return res;
}

static void inode_set(uint32_t id, blkfs_inode *data, blkfs_info *fs)
{
#ifdef FDEV
  Info("Set inode %u on %s", id, fs->dev->name);
#endif

  int blkid = id / fs->inode_per_block;
  int tid = id % fs->inode_per_block;
  blkfs_inode *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, blkid, 0, fs) == fs->block_data_size, "can't read enough");
  memcpy(&items[tid], data, sizeof(blkfs_inode));
  Assert(block_writedata((char *)items, fs->block_data_size, blkid, 0, fs) == fs->block_data_size, "can't write enough");
  pmm->free(items);

#ifdef FDEV
  Info("Setted inode %u on %s", id, fs->dev->name);
#endif
}

static void inode_block_init(int block_id, blkfs_info *fs)
{
  blkfs_inode *entries = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)entries, fs->block_data_size, block_id, 0, fs) == fs->block_data_size, "can't read enough");
  for (int i = 0; i < fs->inode_per_block; i++)
  {
    entries[i].type = EMPTY;
  }
  Assert(block_writedata((char *)entries, fs->block_data_size, block_id, 0, fs) == fs->block_data_size, "can't write enough");
  pmm->free(entries);
}

static void dir_block_init(int block_id, blkfs_info *fs)
{
  dir_entry *entries = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)entries, fs->block_data_size, block_id, 0, fs) == fs->block_data_size, "can't read enough");
  for (int i = 0; i < fs->child_per_dir; i++)
  {
    entries[i].type = FREE;
  }
  Assert(block_writedata((char *)entries, fs->block_data_size, block_id, 0, fs) == fs->block_data_size, "can't write enough");
  pmm->free(entries);
}

static int inode_alloc(int type, blkfs_info *fs)
{
#ifdef FDEV
  Info("Alloc %s inode on %s", type == DIR ? "DIR" : "FILE", fs->dev->name);
#endif

  blkfs_inode *items = pmm->alloc(fs->block_data_size);
  int res = -1;
  for (int bk = 0; bk < fs->inode_block_num; bk++)
  {
    Assert(block_readdata((char *)items, fs->block_data_size, bk, 0, fs) == fs->block_data_size, "can't read enough");
    for (int i = 0; i < fs->inode_per_block; i++)
    {
      if (items[i].type == EMPTY)
      {
        res = bk * fs->inode_per_block + i;
        blkfs_inode *item = items + i;
        item->type = type;
        item->id = res;
        item->size = 0;
        item->refcnt = 0;
        item->block_id = block_alloc(fs);
        if (item->block_id == -1)
        {
          Warning("No more block to alloc");
        }
        else
        {
          if (item->type == DIR)
          {
            dir_block_init(item->block_id, fs);
          }
        }
        break;
      }
    }
    if (res != -1)
    {
      Assert(block_writedata((char *)items, fs->block_data_size, bk, 0, fs) == fs->block_data_size, "can't write enough");
      break;
    }
  }
  pmm->free(items);

#ifdef FDEV
  Info("Alloced %s inode at %d on %s", type == DIR ? "DIR" : "FILE", res, fs->dev->name);
#endif

  return res;
}

static void inode_bfree(uint32_t id, blkfs_info *fs)
{
#ifdef FDEV
  Info("Free inode %u on %s", id, fs->dev->name);
#endif

  int blkid = id / fs->inode_per_block;
  int tid = id % fs->inode_per_block;
  blkfs_inode *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, blkid, 0, fs) == fs->block_data_size, "can't read enough");
  Assert(items[tid].type != EMPTY, "inode has been freed");

  items[tid].type = EMPTY;
  if (items[tid].block_id != -1)
  {
    block_free(items[tid].block_id, fs);
  }

  Assert(block_writedata((char *)items, fs->block_data_size, blkid, 0, fs) == fs->block_data_size, "can't write enough");
  pmm->free(items);

#ifdef FDEV
  Info("Free-ed inode %u on %s", id, fs->dev->name);
#endif
}

static int direntry_find_name(dir_entry *items, const char *name, blkfs_info *fs)
{
  for (int i = 0; i < fs->child_per_dir; i++)
  {
    if (items[i].type == FREE)
      continue;
    if (streq(name, items[i].name))
    {
      return i;
    }
  }
  return -1;
}

static int direntry_find_free(dir_entry *items, blkfs_info *fs)
{
  for (int i = 0; i < fs->child_per_dir; i++)
  {
    if (items[i].type == FREE)
    {
      return i;
    }
  }
  return -1;
}

static fslist *list_dir(blkfs_inode *dir, blkfs_info *fs)
{
  Assert(dir != NULL, "dir is NULL");
  Assert(dir->type == DIR, "dir is not a DIR");

  fslist *head = NULL;
  dir_entry *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't read enough");
  for (int i = 0; i < fs->child_per_dir; i++)
  {
    if (items[i].type == FREE)
      continue;

    fslist *cur = pmm->alloc(sizeof(fslist));
    cur->name = string_create_copy(items[i].name);

    {
      blkfs_inode *node = inode_get(items[i].inode_id, fs);
      if (node->type == DIR)
      {
        cur->type = ITEM_DIR;
      }
      else if (node->type == FILE)
      {
        cur->type = ITEM_FILE;
      }
      else
      {
        cur->type = ITEM_LINK;
      }
      pmm->free(node);
    }

    cur->next = head;
    head = cur;
  }
  pmm->free(items);
  return head;
}

static blkfs_inode *find_filedir(blkfs_inode *dir, const char *name, blkfs_info *fs)
{
  Assert(dir != NULL, "dir is NULL");
  Assert(dir->type == DIR, "dir is not a DIR");

  dir_entry *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't read enough");
  blkfs_inode *res = NULL;
  int ind = direntry_find_name(items, name, fs);
  if (ind != -1)
    res = inode_get(items[ind].inode_id, fs);
  pmm->free(items);
  return res;
}

// return inode_id
static int get_filedir(blkfs_inode *dir, const char *name, blkfs_info *fs)
{
#ifdef FDEV
  Info("Get %s on %s", name, fs->dev->name);
#endif

  Assert(dir != NULL, "dir is NULL");
  Assert(dir->type == DIR, "dir is not a DIR");
  Assert(strlen(name) < INODE_NAME_LEN - 1, "name is too long");

  dir_entry *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't read enough");

  int ind = direntry_find_name(items, name, fs);

  int res = -1;

  if (ind != -1)
  {
    res = items[ind].inode_id;
  }

  pmm->free(items);

#ifdef FDEV
  Info("Getted %s inode %d on %s", name, res, fs->dev->name);
#endif

  return res;
}

// return inode_id
static int create_filedir(blkfs_inode *dir, int type, int link_id, const char *name, blkfs_info *fs)
{
#ifdef FDEV
  Info("Create %s %s on %s", type == DIR ? "DIR" : "FILE", name, fs->dev->name);
#endif

  Assert(dir != NULL, "dir is NULL");
  Assert(dir->type == DIR, "dir is not a DIR");
  Assert(strlen(name) < INODE_NAME_LEN - 1, "name is too long");

  dir_entry *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't read enough");

  int res = -1;
  int lind = direntry_find_name(items, name, fs);
  if (lind == -1)
  {
    int ind = direntry_find_free(items, fs);
    if (ind != -1)
    {
      if (type == LINK)
      {
        res = link_id;
      }
      else
      {
        res = inode_alloc(type, fs);
      }

      blkfs_inode *node = inode_get(res, fs);
      node->refcnt++;
      inode_set(node->id, node, fs);
      pmm->free(node);

      strcpy(items[ind].name, name);
      items[ind].type = USING;
      items[ind].inode_id = res;

      Assert(block_writedata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't write enough");
    }
  }
  else
  {
    Warning("The name %s has exist.", name);
  }

  pmm->free(items);

#ifdef FDEV
  Info("Created %s %s at inode %d on %s", type == DIR ? "DIR" : "FILE", name, res, fs->dev->name);
#endif

  return res;
}

// not recur
static void delete_filedir(blkfs_inode *dir, const char *name, blkfs_info *fs)
{
#ifdef FDEV
  Info("Delete %s on %s", name, fs->dev->name);
#endif

  Assert(dir != NULL, "dir is NULL");
  Assert(dir->type == DIR, "dir is not a DIR");

  dir_entry *items = pmm->alloc(fs->block_data_size);
  Assert(block_readdata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't read enough");
  int ind = direntry_find_name(items, name, fs);
  if (ind != -1)
  {
    blkfs_inode *node = inode_get(items[ind].inode_id, fs);
    node->refcnt--;
    if (node->refcnt == 0)
    {
      inode_bfree(node->id, fs);
    }
    else
    {
      inode_set(node->id, node, fs);
    }
    pmm->free(node);
    items[ind].type = FREE;

    Assert(block_writedata((char *)items, fs->block_data_size, dir->block_id, 0, fs) == fs->block_data_size, "can't write enough");
  }
  pmm->free(items);

#ifdef FDEV
  Info("Deleted %s on %s", name, fs->dev->name);
#endif
}

static blkfs_inode *get_root_inode(blkfs_info *fs)
{
  return inode_get(0, fs);
}

static blkfs_inode *get_path_inode(const char *path, blkfs_info *finfo)
{
  blkfs_inode *bnode = get_root_inode(finfo);
  Assert(bnode != NULL, "root inode get failed");

  int finded = 0;
  const char *cur = path, *nxt = NULL;
  while (1)
  {
    char *name = path_next_name(cur, &nxt);

    if (name == NULL)
    {
      finded = 1;
      break;
    }

#ifdef FDEV
    Warning("Get name '%s'", name);
#endif

    int chi_id = get_filedir(bnode, name, finfo);
    if (chi_id == -1)
    {
      break;
    }
    blkfs_inode *chi = inode_get(chi_id, finfo);
    Assert(chi != NULL, "child inode %d get failed", chi_id);
    pmm->free(bnode);
    bnode = chi;

    if (nxt == NULL)
    {
      finded = 1;
      break;
    }

    cur = nxt;
  }

  if (!finded)
  {
    return NULL;
  }

  return bnode;
}

#pragma endregion

static int inode_access(file *f, int flags)
{
  if (f->inode->ptr == NULL)
  {
    return -1;
  }

  blkfs_inode *node = f->inode->ptr;
  if (node == NULL)
    return -1;
  switch (flags)
  {
  case DIR_EXIST:
    return node->type == DIR ? 0 : -1;
  case FILE_EXIST:
  case FILE_READ:
  case FILE_WRITE:
  case FILE_APPEND:
    return node->type == FILE ? 0 : -1;
  default:
    Warning("No this flags %d", flags);
    return -1;
    break;
  }
  return 0;
}

static int inode_open(file *f, int flags)
{
  blkfs_info *info = f->inode->fs->ptr;

  int res = -1;

  if (f->inode->ptr == NULL && (flags == FILE_WRITE || flags == FILE_APPEND)) // not existed
  {
    const char *path = f->inode->info->path;
    char *parent_dir = path_get_directory(path);
    char *filename = path_get_name(path);
    if (strlen(filename) == 0)
    {
      Warning("The file name is empty: %s", path);
    }
    else
    {
      blkfs_inode *par = get_path_inode(parent_dir, info);
      if (par == NULL)
      {
        Warning("The parent directory is not existed");
      }
      else
      {
        int id = create_filedir(par, FILE, -1, filename, info);
        if (id == -1)
        {
          Warning("Create file failed: %s", path);
        }
        else
        {
          blkfs_inode *node = inode_get(id, info);
          Assert(node != NULL, "inode get failed");
          f->inode->ptr = node;
          res = 0;
        }
      }
      pmm->free(par);
    }
    pmm->free(parent_dir);
    pmm->free(filename);
  }
  else
  {
    res = 0;
  }

  if (res == -1 || inode_access(f, flags) == -1)
    return -1;

  switch (flags)
  {
  case DIR_EXIST:
    Warning("DIR_EXIST can't open");
    return -1;
  case FILE_READ:
    f->flags = flags;
    f->offset = 0;
    break;
  case FILE_WRITE:
    f->flags = flags;
    f->offset = 0;
    break;
  case FILE_APPEND:
    f->flags = FILE_WRITE;
    {
      blkfs_inode *node = f->inode->ptr;
      f->offset = node->size;
    }
    break;
  }
  return res;
}

static int inode_close(file *f)
{
  if (f->inode->ptr == NULL)
  {
    Warning("The file is not existed.");
    return -1;
  }
  return 0;
}

static ssize_t inode_read(file *f, void *buf, size_t size)
{
  if (f->flags != FILE_READ)
  {
    Warning("Not readable file.");
    return -1;
  }
  if (f->inode->ptr == NULL)
  {
    Warning("The file is not existed.");
    return -1;
  }

  blkfs_info *info = f->inode->fs->ptr;
  blkfs_inode *node = f->inode->ptr;

  Assert(node->size >= f->offset, "offset is too large");
  Assert(node->block_id != -1, "The inode do not have block");

  size_t rem = node->size - f->offset;
  size_t count = size > rem ? rem : size;

  size_t readed = block_readdata(buf, count, node->block_id, f->offset, info);

  f->offset += readed;
  return readed;
}

static ssize_t inode_write(file *f, const void *buf, size_t size)
{
  if (f->flags != FILE_WRITE)
  {
    Warning("Not writable file.");
    return -1;
  }
  if (f->inode->ptr == NULL)
  {
    Warning("The file is not existed.");
    return -1;
  }
  blkfs_info *info = f->inode->fs->ptr;
  blkfs_inode *node = f->inode->ptr;

  Assert(node->block_id != -1, "The inode do not have block");

  size_t rem = info->block_data_size - f->offset;
  size_t count = size > rem ? rem : size;

  size_t writen = block_writedata((char *)buf, count, node->block_id, f->offset, info);

  f->offset += writen;
  if (f->offset > node->size)
  {
    node->size = f->offset;
    inode_set(node->id, node, info);
  }
  return writen;
}

// ignore whence, always set
static off_t inode_lseek(file *f, off_t offset, int whence)
{
  if (f->flags != FILE_READ && f->flags != FILE_WRITE)
  {
    Warning("Not seekable file.");
    return -1;
  }
  if (f->inode->ptr == NULL)
  {
    Warning("The file is not existed.");
    return -1;
  }
  blkfs_inode *node = f->inode->ptr;
  offset = offset > node->size ? node->size : offset;
  f->offset = offset;
  return f->offset;
}

// TODO: not support multiply create
static int fs_mkdir(filesystem *fs, const char *name)
{
#ifdef FDEV
  Info("Mkdir %s on %s", name, fs->name);
#endif

  Assert(fs->dev != NULL, "blkfs %s has no dev", fs->name);
  blkfs_info *info = fs->ptr;

  kmt->spin_lock(&info->lock);

  char *parent_dir = path_get_directory(name);
  char *dirname = path_get_name(name);

  blkfs_inode *node = get_path_inode(parent_dir, info);

  int res = -1;

  if (strlen(dirname) != 0)
  {
    if (node != NULL)
    {
      if (node->type == DIR)
      {
        int id = create_filedir(node, DIR, -1, dirname, info);
        if (id == -1)
        {
          Warning("Create dir failed: %s", name);
        }
        else
        {
          res = 0;
        }
      }
      else
      {
        Warning("The parent path is not a directory.");
      }
    }
    else
    {
      Warning("Can't find the parent directory.");
    }
  }
  else
  {
    Warning("The dirname is empty.");
  }

  pmm->free(parent_dir);
  pmm->free(dirname);
  pmm->free(node);

  kmt->spin_unlock(&info->lock);

#ifdef FDEV
  Info("Mkdir-ed %s on %s", name, fs->name);
#endif

  return res;
}

static int fs_rm(filesystem *fs, const char *name)
{
#ifdef FDEV
  Info("Rm %s on %s", name, fs->name);
#endif

  Assert(fs->dev != NULL, "blkfs %s has no dev", fs->name);
  blkfs_info *info = fs->ptr;

  kmt->spin_lock(&info->lock);

  char *parent_dir = path_get_directory(name);
  char *dirname = path_get_name(name);

  blkfs_inode *node = get_path_inode(parent_dir, info);

  int res = -1;

  if (strlen(dirname) != 0)
  {
    if (node != NULL)
    {
      if (node->type == DIR)
      {
        delete_filedir(node, dirname, info);
        res = 0;
      }
      else
      {
        Warning("The parent path is not a directory.");
      }
    }
    else
    {
      Warning("Can't find the parent directory.");
    }
  }
  else
  {
    Warning("The dirname is empty.");
  }

  pmm->free(parent_dir);
  pmm->free(dirname);
  pmm->free(node);

  kmt->spin_unlock(&info->lock);

#ifdef FDEV
  Info("Rm-ed %s on %s", name, fs->name);
#endif

  return res;
}

static int fs_link(filesystem *fs, const char *oldpath, const char *newpath)
{
  Assert(fs->dev != NULL, "blkfs %s has no dev", fs->name);

  blkfs_info *info = fs->ptr;

  kmt->spin_lock(&info->lock);

  int res = -1;

  blkfs_inode *old = get_path_inode(oldpath, info);

  if (old == NULL)
  {
#ifdef FDEV
    Warning("Not found oldpath %s", oldpath);
#endif
  }
  else
  {
    char *dir = path_get_directory(newpath);
    char *name = path_get_name(newpath);
    blkfs_inode *newdir = get_path_inode(dir, info);
    if (newdir == NULL)
    {
#ifdef FDEV
      Warning("Not found new dir %s", dir);
#endif
    }
    else
    {
      int id = create_filedir(newdir, LINK, old->id, name, info);
      if (id == -1)
      {
        Warning("Create %s in %s failed", name, dir);
      }
      else
      {
        res = 0;
      }
      pmm->free(newdir);
    }
    pmm->free(dir);
    pmm->free(name);
  }

  pmm->free(old);

  kmt->spin_unlock(&info->lock);

  return res;
}

static fslist *fs_readdir(filesystem *fs, const char *path)
{
  Assert(fs->dev != NULL, "blkfs %s has no dev", fs->name);

  blkfs_info *info = fs->ptr;

  kmt->spin_lock(&info->lock);

  blkfs_inode *node = get_path_inode(path, info);

  fslist *res = NULL;

  if (node != NULL)
  {
    res = list_dir(node, info);
  }

  pmm->free(node);

  kmt->spin_unlock(&info->lock);

  return res;
}

static void fs_init(filesystem *fs, const char *name, const char *type, device_t *dev)
{
  if (name != NULL)
    fs->name = name;
  if (type != NULL)
    fs->type = type;

  Assert(dev != NULL, "dev of blkfs %s is NULL", fs->name);

  fs->dev = dev;

  blkfs_info *info = pmm->alloc(sizeof(blkfs_info));
  kmt->spin_init(&info->lock, fs->name);

  kmt->spin_lock(&info->lock);

  fs->ptr = info;

  {
    rd_t *rd = fs->dev->ptr;
    info->dev = fs->dev;
    info->full_size = rd->end - rd->start;

    Assert(info->full_size > 0, "The device of blkfs %s is too small: %u bytes", fs->name, info->full_size);

    info->block_size = sizeof(blkfs_block) + BLOCK_DATA_SIZE;
    info->block_data_size = BLOCK_DATA_SIZE;
    info->total_block_num = info->full_size / info->block_size;
    uint64_t inode_size = info->total_block_num * sizeof(blkfs_inode);
    info->inode_block_num = inode_size / BLOCK_DATA_SIZE + (inode_size % BLOCK_DATA_SIZE == 0 ? 0 : 1);
    info->inode_per_block = BLOCK_DATA_SIZE / sizeof(blkfs_inode);
    info->child_per_dir = BLOCK_DATA_SIZE / sizeof(dir_entry);

    Assert(info->inode_block_num < info->total_block_num, "No more block for data.");

#ifdef FDEV
    Info("Blockfs %s on %s info:", fs->name, info->dev->name);
    PassN("  full size: %u", info->full_size);
    PassN("  block size: %u(%u for data)", info->block_size, info->block_data_size);
    PassN("  block number: %u(%u for inode)", info->total_block_num, info->inode_block_num);
    PassN("  inode per block: %u", info->inode_per_block);
    PassN("  child per dir: %u", info->child_per_dir);
#endif
  }

  if (block_check(0, info) == 0) // exists blkfs
  {

#ifdef FDEV
    Info("Exists blkfs on %s", dev->name);
#endif

    for (int i = 0; i < info->total_block_num; i++)
    {
      Assert(block_check(i, info) == 0, "Block %d check for %s failed.", i, dev->name);
    }
  }
  else
  {
    for (int i = 0; i < info->total_block_num; i++)
      block_init(i, info);
    for (int i = 0; i < info->inode_block_num; i++)
    {
      blkfs_block *blk = block_getinfo(i, info);
      blk->type = USING;
      block_setinfo(blk, i, info);
      pmm->free(blk);
      inode_block_init(i, info);
    }
    int rootdir = inode_alloc(DIR, info);

#ifdef FDEV
    Info("Create rootdir inode at id %d for %s", rootdir, fs->name);
#endif

    Assert(rootdir == 0, "rootdir inode not at 0, it's at %d", rootdir);

    blkfs_inode *root = get_root_inode(info);
    root->refcnt++;
    inode_set(root->id, root, info);

    Assert(root->block_id == info->inode_block_num, "Create root dir failed, expected %d but actual %d", info->inode_block_num, rootdir);
    dir_block_init(root->block_id, info);

    pmm->free(root);
  }

  kmt->spin_unlock(&info->lock);
}

static inode *fs_lookup(filesystem *fs, const char *path, int flags)
{
  Assert(fs->dev != NULL, "blkfs %s has no dev", fs->name);
  Assert(path[0] == '/', "path %s must start with /", path);

#ifdef FDEV
  Info("Look up on blkfs for %s", path);
#endif

  blkfs_info *finfo = fs->ptr;

  kmt->spin_lock(&finfo->lock);

  blkfs_inode *bnode = get_path_inode(path, finfo);

#ifdef FDEV
  Info("Finded inode %p by %s on %s", bnode, path, fs->name);
#endif

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
      .ptr = bnode,
      .ops = ops,
      .info = info,
  };

  kmt->spin_unlock(&finfo->lock);

  return node;
}

static int fs_close(filesystem *fs, inode *inode)
{
  Assert(fs->dev != NULL, "blkfs %s has no dev", fs->name);

#ifdef FDEV
  Info("Close %s on blkfs", inode->info->path);
#endif

  blkfs_info *info = fs->ptr;
  kmt->spin_lock(&info->lock);

  if (inode == NULL)
  {
    Warning("Close a NULL inode");
    return -1;
  }

  blkfs_inode *bnode = inode->ptr;

  pmm->free(bnode);

  inode_free(inode);

  kmt->spin_unlock(&info->lock);
  return 0;
}

filesystem *blkfs_create(device_t *dev)
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