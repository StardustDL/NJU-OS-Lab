#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#include <common.h>
#include <klib.h>
#include <debug.h>

// #define MDEV
// #define NAIVE

static uintptr_t pm_start, pm_end;

static spinlock_t pmm_lock;

const uint32_t node_magic = 12345678, block_magic = 31579846;

typedef struct _node
{
  uint32_t magic;
  uint32_t size;
  struct _node *next;
} node;

static node *head = NULL;

static void node_init(node *nd, uint32_t size)
{
  nd->magic = node_magic;
  nd->next = NULL;
  nd->size = size;
}

// return the first node with first_size
static node *node_split(node *nd, uint32_t first_size)
{
  Assert(nd != NULL, "split NULL");
  if (nd->size < first_size + sizeof(node))
  {
    Warning("No enough space to split node: have %d but need %d.", nd->size, first_size + sizeof(node));
    return NULL;
  }
  uint32_t full_size = nd->size;
  node *next = nd->next;
  node *new_node = (node *)(((uintptr_t)nd) + sizeof(node) + first_size);
  node_init(new_node, full_size - first_size - sizeof(node));
  nd->next = new_node;
  nd->size = first_size;
  new_node->next = next;

  return nd;
}

// if can combine, return nd, else NULL
static node *node_combine(node *nd)
{
  Assert(nd != NULL, "combine NULL");
  if ((((uintptr_t)nd) + sizeof(node) + nd->size) != ((uintptr_t)nd->next))
  {
    return NULL;
  }
  Assert(nd->next != NULL, "combine next NULL");
  nd->size = nd->size + nd->next->size + sizeof(node);
  nd->next = nd->next->next;
  return nd;
}

static void assert_no_cycle()
{
  node *cur = head;
  while (cur != NULL)
  {
    Assert(cur->magic == node_magic, "node check failed %u, at %p", cur->magic, cur);
    if (cur == cur->next)
    {
      Panic("Cycle!!!");
    }
    cur = cur->next;
  }
}

// return the first node->size>=size node
static node *node_find_size(uint32_t size, node **last)
{
  node *cur = head;
  *last = NULL;
  while (cur != NULL)
  {
    if (cur->size >= size)
    {
      return cur;
    }
    *last = cur;
    cur = cur->next;
  }
  return NULL;
}

// return the last pos(node)<=pos node
static node *node_find_position(uintptr_t pos)
{
  node *cur = head;
  node *ans = NULL;
  while (cur != NULL)
  {
    if ((uintptr_t)cur <= pos)
    {
      ans = cur;
    }
    cur = cur->next;
  }
  return ans;
}

typedef struct _block
{
  uint32_t magic;
  uint32_t size;
  uintptr_t data;
} block;

static void block_init(block *bk, uint32_t size)
{
  bk->magic = block_magic;
  bk->size = size;
  bk->data = block_magic;
}

static void block_clear(block *bk)
{
  memset((void *)(((uintptr_t)bk) + sizeof(block)), 0, bk->size);
}

#ifdef NAIVE

uintptr_t naive_pos;

#endif

static void init()
{
  Assert(sizeof(node) == sizeof(block), "node size != block size");

  pm_start = (uintptr_t)_heap.start;
  pm_end = (uintptr_t)_heap.end;

  Info("pm_start %u, pm_end %u, size %u KB", pm_start, pm_end, (pm_end - pm_start) / 1024);

#ifdef NAIVE

  naive_pos = pm_start;

#else

  head = (node *)pm_start;
  node_init(head, (pm_end - pm_start - sizeof(node)));

#endif

  kmt->spin_init(&pmm_lock, "pmm-spinlock");
}

static void *alloc(size_t size)
{
#ifdef MDEV
  Info("Want to alloc %d bytes.", size);
#endif

  kmt->spin_lock(&pmm_lock);

  void *res = NULL;

#ifdef NAIVE

  if (naive_pos + size > pm_end)
  {
    Panic("No space to alloc");
  }
  res = (void *)naive_pos;

  memset(res, 0, size);

  naive_pos += size;

#else

  node *last = NULL;
  node *nd = node_find_size(size + sizeof(node), &last);

  if (nd == NULL)
  {
    Warning("No enough space for %d bytes.", size + sizeof(node));
  }
  else
  {
    Assert(nd->magic == node_magic, "Not a node");
    Assert(node_split(nd, size) != NULL, "split failed");
    if (nd == head)
    {
      Assert(last == NULL, "last node is not NULL when find head");
      head = nd->next;
    }
    else
    {
      Assert(last != NULL, "last node is not NULL when find medium");
      last->next = nd->next;
    }
    block *bk = (block *)nd;
    block_init(bk, size);
    block_clear(bk);
    res = (void *)((uintptr_t)bk + sizeof(block));
  }

#ifdef MDEV
  assert_no_cycle();
#endif

#endif

  kmt->spin_unlock(&pmm_lock);

#ifdef MDEV
  Pass("Alloced %d bytes at %p.", size, res);
#endif

  return res;
}

static void free(void *ptr)
{
#ifdef MDEV
  Info("Want to free at %p.", ptr);
#endif

  if (ptr == NULL)
    return;

  kmt->spin_lock(&pmm_lock);

#ifdef NAIVE

#else

  block *bk = (block *)((uintptr_t)ptr - sizeof(block));
  Assert(bk->magic == block_magic, "Not a block");
  node *nd = (node *)bk;
  node_init(nd, bk->size);
  node *tnd = node_find_position((uintptr_t)bk);
  if (tnd == NULL)
  {
    nd->next = head;
    head = nd;
    node_combine(nd);
  }
  else
  {
    nd->next = tnd->next;
    tnd->next = nd;
    node *cmnd = node_combine(tnd);
    if (cmnd == NULL)
    {
      node_combine(nd);
    }
    else
    {
      node_combine(tnd);
    }
  }

#ifdef MDEV
  assert_no_cycle();
#endif

#endif

  kmt->spin_unlock(&pmm_lock);

#ifdef MDEV
  Pass("Freed at %p.", ptr);
#endif
}

MODULE_DEF(pmm){
    .init = init,
    .alloc = alloc,
    .free = free,
};
