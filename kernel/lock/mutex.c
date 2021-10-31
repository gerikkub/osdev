
#include <stdint.h>
#include <stdbool.h>

#include "kernel/lock/lock.h"
#include "kernel/task.h"
#include "kernel/assert.h"

#include "kernel/lib/vmalloc.h"

typedef struct {
    uint64_t* waiter_list;
    uint64_t num_waiters;
    uint64_t max_num_waiters;
    uint64_t waiter_head;
    uint64_t waiter_tail;
} mutex_t;

bool mutex_try_acquire(lock_t* lock, bool should_wait) {
    uint64_t tid;
    uint64_t crit;
    bool got_lock;
    mutex_t* mutex_ctx = lock->lock_ctx;

    BEGIN_CRITICAL(crit);

    GET_CURR_TID(tid);


    // This will break with multiple CPUs
    if (lock->tid == 0) {
        lock->tid = tid;
        got_lock = true;
    } else {
        got_lock = false;
    }

    if (!got_lock && should_wait) {
        mutex_ctx->num_waiters++;
        ASSERT(mutex_ctx->num_waiters <= mutex_ctx->max_num_waiters);
        mutex_ctx->waiter_list[mutex_ctx->waiter_head] = tid;
        mutex_ctx->waiter_head = (mutex_ctx->waiter_head + 1) % mutex_ctx->max_num_waiters;
    }

    END_CRITICAL(crit);

    return got_lock;
}

uint64_t mutex_release(lock_t* lock) {
    uint64_t crit;
    uint64_t tid;
    uint64_t next_tid = 0;
    mutex_t* mutex_ctx = lock->lock_ctx;

    BEGIN_CRITICAL(crit);

    GET_CURR_TID(tid);

    ASSERT(lock->tid == tid);

    if (mutex_ctx->num_waiters > 0) {
        mutex_ctx->num_waiters--;
        next_tid = mutex_ctx->waiter_list[mutex_ctx->waiter_tail];
        mutex_ctx->waiter_tail = (mutex_ctx->waiter_tail + 1) % mutex_ctx->max_num_waiters;
        lock->tid = next_tid;
    }

    END_CRITICAL(crit);

    return next_tid;
}

void mutex_init(lock_t* lock, uint64_t max_num_waiters) {

    ASSERT(lock != NULL);

    mutex_t* mutex_ctx = vmalloc(sizeof(mutex_t));

    mutex_ctx->waiter_list = vmalloc(sizeof(uint64_t) * max_num_waiters);
    mutex_ctx->num_waiters = 0;
    mutex_ctx->max_num_waiters = max_num_waiters;
    mutex_ctx->waiter_head = 0;
    mutex_ctx->waiter_tail = 0;

    lock_init(lock, mutex_try_acquire, mutex_release, mutex_ctx);
}

void mutex_destroy(lock_t* lock) {

    ASSERT(lock != NULL);
    ASSERT(lock->lock_ctx != NULL);
    ASSERT(lock->tid == 0);

    mutex_t* mutex_ctx = lock->lock_ctx;

    vfree(mutex_ctx->waiter_list);
    vfree(mutex_ctx);

    lock->try_acquire = NULL;
    lock->release = NULL;
}