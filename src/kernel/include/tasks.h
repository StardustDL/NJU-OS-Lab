#ifndef __TASKS_H__
#define __TASKS_H__

typedef struct _task_node
{
  task_t *task;
  struct _task_node *next;
} task_node;

task_t *current_task();

task_node* get_task_list();

task_t *task_lookup(const char* name);

#endif