#include <stdio.h>
#include <stdint.h>
#include <ucontext.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "co.h"

// #define DEBUG

#ifdef DEBUG
#include <common/debug.h>
#endif

#define KB *1024LL
#define MB KB * 1024LL
#define GB MB * 1024LL

#define STACK_SIZE 4 MB
#define MAX_CO 256

typedef enum
{
  FREE,
  INIT,
  RUN,
  SUSPEND,
  BEWAIT
} co_state;

struct co
{
  int id;
  uint8_t stack[STACK_SIZE];
  func_t func;
  void *arg;
  co_state state;
  ucontext_t ctx, pctx;
  char *name;
};

struct co coroutines[MAX_CO];
static int current_co;

void co_func_wrap(struct co *thd)
{
  assert(thd != NULL);
  thd->func(thd->arg);
  thd->state = FREE;
  current_co = -1;
}

static int get_co(co_state state, int start)
{
  assert(start >= 0 && start < MAX_CO);
  for (int i = 0; i < MAX_CO; i++)
  {
    int nxt = (start + i) % MAX_CO;
    if (coroutines[nxt].state == state)
      return nxt;
  }
  return -1;
}

static int create_co(const char *name, func_t func, void *arg)
{
  int id = get_co(FREE, rand() % MAX_CO);
  if (id == -1)
    return -1;

#ifdef DEBUG
  Info("Create %s @ %d", name, id);
#endif

  struct co *thd = coroutines + id;
  memset(thd->stack, 0, STACK_SIZE);
  thd->state = INIT;
  thd->func = func;
  thd->arg = arg;
  thd->name = (char *)name;

  getcontext(&thd->ctx);
  thd->ctx.uc_stack.ss_sp = thd->stack;
  thd->ctx.uc_stack.ss_size = STACK_SIZE;
  thd->ctx.uc_stack.ss_flags = 0;
  thd->ctx.uc_link = &thd->pctx;

  makecontext(&thd->ctx, (void (*)(void))(co_func_wrap), 1, thd);

  return id;
}

static void resume_co(int id)
{
  if (id != -1)
  {
    struct co *thd = coroutines + id;
    assert(thd->state != FREE);

    if (thd->state == SUSPEND || thd->state == INIT)
    {
      thd->state = RUN;
    }
    else if (thd->state == BEWAIT)
    {
    }
    else
    {
      printf("resume_co: unvalid state: %d(%s) with state %d\n", thd->id, thd->name, thd->state);
      assert(0);
    }

    int old_current = current_co;
    current_co = id;

#ifdef DEBUG
    Info("Resume %d -> %d (%s)", old_current, id, thd->name);
#endif

    swapcontext(&thd->pctx, &thd->ctx);

#ifdef DEBUG
    Info("Resume back %d (%s) -> %d", id, thd->name, old_current);
#endif

    current_co = old_current;
  }
}

void co_init()
{
  current_co = -1;
  for (int i = 0; i < MAX_CO; i++)
  {
    coroutines[i].state = FREE;
    coroutines[i].id = i;
  }
}

struct co *co_start(const char *name, func_t func, void *arg)
{
  int id = create_co(name, func, arg);
  if (id == -1)
    return NULL;

  resume_co(id);

  return coroutines + id;
}

void co_yield()
{
  if (current_co != -1)
  {
    struct co *thd = coroutines + current_co;
    if (thd->state == BEWAIT) // Do not back to parent
    {
      int other = get_co(SUSPEND, rand() % MAX_CO);
      if (other == -1)
        return;

#ifdef DEBUG
      Warning("Yield(O) %s @ %d", thd->name, thd->id);
#endif

      resume_co(other);
    }
    else if (thd->state == RUN)
    {
#ifdef DEBUG
      Warning("Yield(P) %s @ %d", thd->name, thd->id);
#endif

      thd->state = SUSPEND;
      swapcontext(&thd->ctx, &thd->pctx);
    }
    else
    {
      printf("co_yield: unvalid state: %d(%s) with state %d\n", thd->id, thd->name, thd->state);
      assert(0);
    }
  }
}

void co_wait(struct co *thd)
{
  if (thd->state == FREE)
    return;
  thd->state = BEWAIT;

#ifdef DEBUG
  Warning("Wait %s @ %d", thd->name, thd->id);
#endif

  resume_co(thd->id);
}
