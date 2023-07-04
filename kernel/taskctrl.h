
#ifndef __TASKCTRL_H__
#define __TASKCTRL_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/task.h"

void task_ops_waited(task_t* task, task_t* target_task, void* ctx);
int64_t syscall_taskctrl(uint64_t tid, uint64_t x1, uint64_t x2, uint64_t x3);

#endif
