// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wunused-function"

#include <common.h>
#include <klib.h>
#include <test.h>
#include <debug.h>
#include <devices.h>

// #define ODEV

typedef struct _irq_handler
{
  int seq;
  int event;
  handler_t handler;
  struct _irq_handler *next;
} irq_handler;

static irq_handler *head = NULL;

static spinlock_t trap_lock;

// return the last hand->seq<=seq hand
static irq_handler *find_handler_seq(int seq)
{
  irq_handler *cur = head;
  irq_handler *ans = NULL;
  while (cur != NULL)
  {
    if (cur->seq <= seq)
    {
      ans = cur;
    }
    cur = cur->next;
  }
  return ans;
}

static void idle(void *args)
{
  while (1)
    _yield();
}

// #define DTTY

#ifdef DTTY

static void echo_task(void *name)
{
  device_t *tty = dev_lookup(name);
  while (1)
  {
    char line[128], text[128];
    sprintf(text, "(%s) $ ", name);
    tty->ops->write(tty, 0, text, strlen(text)); //TODO: HANG
    int nread = tty->ops->read(tty, 0, line, sizeof(line));
    line[nread - 1] = '\0';
    sprintf(text, "Echo: %s.\n", line);
    tty->ops->write(tty, 0, text, strlen(text));
  }
}

#endif

void shell_task(void *);

static void init()
{
#ifdef ODEV
  InfoN("OS initializing");
#endif

  head = NULL;

  pmm->init();
  kmt->init();
  dev->init();
  vfs->init();

  kmt->spin_init(&trap_lock, "os-trap-lock");

  kmt->create(pmm->alloc(sizeof(task_t)), "idle-1", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-2", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-3", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-4", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-5", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-6", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-7", idle, NULL);
  kmt->create(pmm->alloc(sizeof(task_t)), "idle-8", idle, NULL);

  kmt->create(pmm->alloc(sizeof(task_t)), "shell-1", shell_task, (void *)1);
  kmt->create(pmm->alloc(sizeof(task_t)), "shell-2", shell_task, (void *)2);
  kmt->create(pmm->alloc(sizeof(task_t)), "shell-3", shell_task, (void *)3);
  kmt->create(pmm->alloc(sizeof(task_t)), "shell-4", shell_task, (void *)4);

#ifdef DTTY

  kmt->create(pmm->alloc(sizeof(task_t)), "print-1", echo_task, "tty1");
  kmt->create(pmm->alloc(sizeof(task_t)), "print-2", echo_task, "tty2");
  kmt->create(pmm->alloc(sizeof(task_t)), "print-3", echo_task, "tty3");
  kmt->create(pmm->alloc(sizeof(task_t)), "print-4", echo_task, "tty4");

#endif

  // #ifdef ODEV
  PassN("OS initialized");
  // #endif
}

static void hello()
{
  printf("Hello from CPU #%d\n", _cpu());
}

static void run()
{
#ifdef ODEV
  InfoN("OS starting at cpu %d", _cpu());
#endif

  hello();

#ifdef ODEV
  InfoN("Enable intr on cpu %d", _cpu());
#endif

  _intr_write(1);

  while (1)
  {
    _yield();
  }

#ifdef ODEV
  InfoN("OS exited at cpu %d", _cpu());
#endif
}

static _Context *trap(_Event ev, _Context *context)
{
  kmt->spin_lock(&trap_lock);

#ifdef ODEV
  InfoN("OS trap on %d, event:%d", _cpu(), ev.event);
#endif

  _Context *ret = context;
  irq_handler *cur = head;
  while (cur != NULL)
  {
    if (cur->event == _EVENT_NULL || cur->event == ev.event)
    {
      _Context *next = cur->handler(ev, context);

#ifdef ODEV
      Info("Finded irq handler for event(%d), context(%p)", cur->event, next);
#endif

      if (next != NULL)
        ret = next;
    }
    cur = cur->next;
  }

  kmt->spin_unlock(&trap_lock);
  return ret;
}

// TODO: Is this be concurracy?
static void on_irq(int seq, int event, handler_t handler)
{
#ifdef ODEV
  Info("IRQ register: seq(%d), event(%d)", seq, event);
#endif

  irq_handler *ih = (irq_handler *)pmm->alloc(sizeof(irq_handler));
  ih->seq = seq;
  ih->event = event;
  ih->handler = handler;

  irq_handler *h = find_handler_seq(seq);
  if (h == NULL)
  {
    ih->next = head;
    head = ih;
  }
  else
  {
    ih->next = h->next;
    h->next = ih;
  }
}

MODULE_DEF(os){
    .init = init,
    .run = run,
    .trap = trap,
    .on_irq = on_irq,
};
