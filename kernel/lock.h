
#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/bitutils.h"

#define BEGIN_CRITICAL(x) \
    do { \
    READ_SYS_REG("DAIF", x);  \
    uint64_t __val = BIT(8) | /* Serror Mask */ \
                     BIT(7) | /* IRQ Mask */ \
                     BIT(6);  /* FIQ Mask */ \
    WRITE_SYS_REG("DAIF", __val); \
    }

#define END_CRITICAL(x) \
    do { \
    WRITE_SYS_REG("DAIF", x); \
    }

typedef struct {
    uint64_t tid;
    uint64_t waiting;
} lock_t;

void lock_init(lock_t* lock);
void lock_acquire(lock_t* lock);
void lock_release(lock_t* lock);
bool lock_try_release(lock_t* lock);


#endif