
#include <stdint.h>
#include <stdbool.h>


#include "kernel/lock.h"
#include "kernel/assert.h"


void lock_init(lock_t* lock) {
    ASSERT(lock != NULL);

    lock->tid = 0; // Not held
    lock->waiting = 0;
}

void lock_try_acquire(lock_t* lock) {
    ASSERT(lock != NULL);

    uint64_t tid;

    GET_CURR_TID(tid);

    uint64_t crit;
    bool got_lock;
    BEGIN_CRITICAL(crit);

    if (lock->tid == 0) {
        lock->tid = tid;
        got_lock = true;
    } else {
        got_lock = false;
    }
    asm ("dmb");
    END_CRITICAL(crit);

    return got_lock;
}

void lock_acquire(lock_t* lock) {

    do {
        bool got_lock = lock_try_acquire(lock);

        if (!got_lock) {
            wait_ctx_t ctx;
            ctx.lock.lock_ptr = lock;
            task_wait(get_active_task(), WAIT_LOCK, ctx);
        }
    } while (!got_lock);
}

void lock_release(lock_t)