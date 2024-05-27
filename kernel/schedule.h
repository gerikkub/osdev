
#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <stdint.h>

#include "kernel/lib/lstruct.h"
#include "kernel/task.h"

void schedule_init(void);
void schedule(void);

void task_requeue(task_t* task);

#endif