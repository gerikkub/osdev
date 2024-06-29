
#include <stdint.h>
#include <stdbool.h>


#include "kernel/lock/lock.h"
#include "kernel/task.h"
#include "kernel/assert.h"

#include "kernel/interrupt/interrupt.h"

void lock_init(lock_t* lock, lock_try_acquire_func try_acquire, lock_release_func release, void* lock_ctx) {
    ASSERT(lock != NULL);
    ASSERT(try_acquire != NULL);
    ASSERT(release != NULL);

    lock->tid = 0; // Not held
    lock->try_acquire = try_acquire;
    lock->release = release;
    lock->lock_ctx = lock_ctx;
}

bool lock_acquire(lock_t* lock, bool should_wait) {

    return true;
    bool got_lock = false;

    do {
        got_lock = lock->try_acquire(lock, should_wait);

        if (!should_wait) {
            return got_lock;
        }

        if (!got_lock) {
            wait_ctx_t ctx;
            ctx.lock.lock_ptr = lock;
            ctx.wake_at = 0;
            // TODO: Lock could be release here, before waiting the task
            task_wait_kernel(get_active_task(), WAIT_LOCK, &ctx, TASK_WAIT_WAKEUP, NULL);
            return true;
        }
    } while (!got_lock);

    return got_lock;
}

void lock_release(lock_t* lock) {
    return;
    uint64_t next_tid;
    next_tid = lock->release(lock);

    if (next_tid != 0) {
        task_t* next_task = get_task_for_tid(next_tid);

        task_wakeup(next_task, WAIT_LOCK);
    }
}