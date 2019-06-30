#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <common.h>
#include <klib.h>
#include <debug.h>
#include <tasks.h>

// #define MDEV

#define MAX_CPU 8
#define STACK_SIZE 8192

static struct
{
  int count;
  int hasIF;
} cpu_cli[MAX_CPU];

static void pushcli()
{
  int oldIF = _intr_read();
  _intr_write(0);
  if (cpu_cli[_cpu()].count == 0)
  {
    Assert(cpu_cli[_cpu()].hasIF == 0, "cpu_cli[%d] is not init state", _cpu());
    cpu_cli[_cpu()].hasIF = oldIF;
#ifdef MDEV
    if (oldIF != 0)
      WarningN("CLI on %d", _cpu());
#endif
  }
  cpu_cli[_cpu()].count++;
}

static void popcli()
{
  if (_intr_read())
    Panic("%s", "popcli when cli enabled");
  if (--cpu_cli[_cpu()].count < 0)
    Panic("%s", "popcli too many times");
  if (cpu_cli[_cpu()].count == 0 && cpu_cli[_cpu()].hasIF)
  {
#ifdef MDEV
    WarningN("STI on %d", _cpu());
#endif
    cpu_cli[_cpu()].hasIF = 0;
    _intr_write(1);
  }
}

static spinlock_t task_lock;
static spinlock_t context_lock;

static task_node *head = NULL;
static task_node *current[MAX_CPU];
static uint32_t task_count = 0;

static task_node *_current()
{
  return current[_cpu()];
}

#pragma region implement of tasks.h

task_t *current_task()
{
  task_node *tn = _current();
  Assert(tn != NULL, "task node is NULL");
  return tn->task;
}

task_node *get_task_list()
{
  return head;
}

task_t *task_lookup(const char *name)
{
  task_node *cur = head;
  while (cur != NULL)
  {
    if (streq(cur->task->name, name))
    {
      return cur->task;
    }
    cur = cur->next;
  }
  return NULL;
}

#pragma endregion

static task_node *get_task(int position)
{
  task_node *cur = head;
  while (cur != NULL && position > 0)
  {
    position--;
    cur = cur->next;
  }
  return cur;
}

static int find_task_index(task_node *target)
{
  task_node *cur = head;
  int ans = 0;
  while (cur != NULL)
  {
    if (cur == target)
    {
      return ans;
    }
    ans++;
    cur = cur->next;
  }
  return -1;
}

#ifdef MDEV
static void assert_no_cycle(task_node *head)
{
  task_node *cur = head;
  while (cur != NULL)
  {
    if (cur == cur->next)
    {
      Panic("Cycle!!!");
    }
    cur = cur->next;
  }
}
#endif

static task_node *get_last_task(task_node *head)
{
#ifdef MDEV
  assert_no_cycle(head);
#endif

  task_node *cur = head;
  task_node *ans = cur;
  while (cur != NULL)
  {
    ans = cur;
    cur = cur->next;
  }
  return ans;
}

static _Context *context_save(_Event ev, _Context *context)
{
  // if (ev.event == _EVENT_IRQ_TIMER || ev.event == _EVENT_YIELD)
  {
    kmt->spin_lock(&context_lock);

    task_node *cur = _current();
    if (cur != NULL)
      cur->task->context = *context;

    kmt->spin_unlock(&context_lock);
  }
  return NULL;
}

static _Context *context_switch(_Event ev, _Context *context)
{
  // if (ev.event == _EVENT_IRQ_TIMER || ev.event == _EVENT_YIELD)
  {
    kmt->spin_lock(&context_lock);

    int index = find_task_index(_current()) + 1;
    task_node *tk = get_task(index % task_count);
    Assert(tk != NULL, "No task to switch.");

    task_node *cur = _current();
    if (cur != NULL)
    {
      Assert(cur->task->state == Working, "Not working when switch from.");
      cur->task->state = Free;
    }

    while (1)
    {
      if (tk->task->state == Free && tk->task->cpu == _cpu()) // Free or current task(just released)
      {
        break;
      }
      index = (index + 1) % task_count;
      tk = get_task(index);
    }

    tk->task->state = Working;
    tk->task->cpu = _cpu();
    current[_cpu()] = tk;

#ifdef MDEV
    if (strncmp("idle", tk->task->name, 4) != 0)
    {
      Info("Switch to task %s on cpu %d.", tk->task->name, tk->task->cpu);
    }
#endif

    kmt->spin_unlock(&context_lock);
    return &tk->task->context;
  }
  return context;
}

static void init()
{
#ifdef MDEV
  InfoN("Module KMT initializing");
#endif

  head = NULL;

  for (int i = 0; i < MAX_CPU; i++)
  {
    cpu_cli[i].count = 0;
    cpu_cli[i].hasIF = 0;
    current[i] = NULL;
  }

  kmt->spin_init(&task_lock, "kmt-task-lock");
  kmt->spin_init(&context_lock, "kmt-context-lock");

  os->on_irq(-10000, _EVENT_NULL, context_save);
  os->on_irq(10000, _EVENT_NULL, context_switch);

#ifdef MDEV
  PassN("Module KMT initialized");
#endif
}

static int create(task_t *task, const char *name, void (*entry)(void *arg), void *arg)
{
  task->name = name;
  task->entry = entry;
  task->arg = arg;
  task->state = Free;
  task->cpu = task_count % _ncpu();
  for (int i = 0; i < N_FILE; i++)
    task->fildes[i] = NULL;

  task->stack = pmm->alloc(STACK_SIZE);
  Assert(task->stack != NULL, "No space for task %s's stack.", task->name);
  task->context = *_kcontext((_Area){.start = task->stack, .end = task->stack + STACK_SIZE - 1}, entry, arg);
  task_node *tn = pmm->alloc(sizeof(task_node));
  tn->task = task;
  tn->next = NULL;

  kmt->spin_lock(&task_lock);
  task_count += 1;
  tn->task->tid = task_count;

  if (head == NULL)
  {
    head = tn;
  }
  else
  {
    task_node *last = get_last_task(head);
    Assert(last != NULL, "last is NULL");
    last->next = tn;
  }
  kmt->spin_unlock(&task_lock);

#ifdef MDEV
  Info("Task %s created", task->name);
#endif
  return 0;
}

static void teardown(task_t *task)
{
  Warning("Not implemented");
}

static bool is_holding_spin(spinlock_t *lk)
{
  pushcli();
  int r = lk->locked && lk->cpu == _cpu();
  popcli();
  return r;
}

static void spin_init(spinlock_t *lk, const char *name)
{
  lk->name = name;
  lk->cpu = -1;
  lk->locked = 0;

#ifdef MDEV
  Info("Spin_lock %s initialized", lk->name);
#endif
}

static void spin_lock(spinlock_t *lk)
{
#ifdef MDEV
  Info("Lock %s from %s", lk->name, _current() == NULL ? "" : current_task()->name);
#endif

  pushcli();

  // TODO: the implement of is_holding_spin, only compare cpu, but not task
  if (is_holding_spin(lk))
    Panic("acquire %s when holding from %s on cpu %d", lk->name, _current() == NULL ? "" : current_task()->name, _cpu());

  while (_atomic_xchg(&lk->locked, 1) != 0)
    ;

  lk->cpu = _cpu();

#ifdef MDEV
  Pass("Locked %s", lk->name);
#endif
}

static void spin_unlock(spinlock_t *lk)
{
#ifdef MDEV
  Info("Unlock %s", lk->name);
#endif

  if (!is_holding_spin(lk))
    Panic("release %s when not holding", lk->name);

  lk->cpu = -1;

  _atomic_xchg(&lk->locked, 0);

  popcli();

#ifdef MDEV
  Pass("Unlocked %s", lk->name);
#endif
}

static void sem_init(sem_t *sem, const char *name, int value)
{
  sem->name = name;
  sem->value = value;
  spin_init(&sem->lock, name);

#ifdef MDEV
  Info("Semaphore %s initialized", sem->name);
#endif
}

static void sem_wait(sem_t *sem)
{
  Assert(_current() != NULL, "Wait on no cpu.");

  spin_lock(&sem->lock);

#ifdef MDEV
  Info("Wait %s by %s with value %d", sem->name, current_task()->name, sem->value);
#endif

  while (1)
  {
    if (sem->value > 0)
    {
      sem->value--;
      break;
    }
#ifdef MDEV
    Warning("Waiting %s by %s with value %d", sem->name, current_task()->name, sem->value);
#endif
    spin_unlock(&sem->lock);
    _yield();
    spin_lock(&sem->lock);
  }

#ifdef MDEV
  Pass("Waited %s", sem->name);
#endif

  spin_unlock(&sem->lock);
}

static void sem_signal(sem_t *sem)
{
  Assert(_current() != NULL, "Signal on no cpu.");

#ifdef MDEV
  Info("Signal %s by %s", sem->name, current_task()->name);
#endif

  spin_lock(&sem->lock);

  sem->value++;
  spin_unlock(&sem->lock);

#ifdef MDEV
  Pass("Signaled %s", sem->name);
#endif
}

MODULE_DEF(kmt){
    .init = init,
    .create = create,
    .teardown = teardown,
    .spin_init = spin_init,
    .spin_lock = spin_lock,
    .spin_unlock = spin_unlock,
    .sem_init = sem_init,
    .sem_wait = sem_wait,
    .sem_signal = sem_signal,
};