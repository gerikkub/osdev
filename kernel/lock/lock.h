
#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>
#include <stdbool.h>

#include "stdlib/bitutils.h"

struct lock_t_;

typedef bool (*lock_try_acquire_func)(struct lock_t_* lock, bool should_wait);
typedef uint64_t (*lock_release_func)(struct lock_t_* lock);

typedef struct lock_t_ {
    uint64_t tid;
    lock_try_acquire_func try_acquire;
    lock_release_func release;
    void* lock_ctx;
} lock_t;

void lock_init(lock_t* lock, lock_try_acquire_func try_acquire, lock_release_func release, void* lock_ctx);
bool lock_acquire(lock_t* lock, bool should_wait);
void lock_release(lock_t* lock);


#endif