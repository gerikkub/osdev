
#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/lock/lock.h"

void mutex_init(lock_t* lock, uint64_t max_num_waiters);
void mutex_destroy(lock_t* lock);

#endif
