
#include <stdint.h>
#include <stdbool.h>

#include "stdlib/bitutils.h"
#include "kernel/exception.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/console.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/elapsedtimer.h"

#include "kernel/interrupt/interrupt.h"

typedef struct {
    irq_handler fn;
    void* ctx;
    uint64_t awaiter_tid;
    bool awaited;
    uint64_t intid;
    elapsedtimer_t profile_time;
} intc_irq_handler_ctx_t;

typedef struct {
    bool registered;
    intc_funcs_t intc_fn;
    void* intc_ctx;

    intc_irq_handler_ctx_t* handlers;
    uint64_t num_handlers;
} intc_irq_ctx_t;

static intc_irq_ctx_t s_interrupt_ctx;

void interrupt_init(void) {
    s_interrupt_ctx.registered = false;
}

void interrupt_register_controller(intc_funcs_t* funcs, void* ctx, uint64_t num_handlers) {
    s_interrupt_ctx.registered = true;
    s_interrupt_ctx.intc_fn = *funcs;
    s_interrupt_ctx.intc_ctx = ctx;

    s_interrupt_ctx.handlers = vmalloc(num_handlers * sizeof(intc_irq_handler_ctx_t));
    s_interrupt_ctx.num_handlers = num_handlers;

    for (uint64_t idx = 0; idx < num_handlers; idx++) {
        s_interrupt_ctx.handlers[idx].fn = NULL;
        s_interrupt_ctx.handlers[idx].awaiter_tid = 0;
        s_interrupt_ctx.handlers[idx].awaited = false;
        elapsedtimer_clear(&s_interrupt_ctx.handlers[idx].profile_time);
    }
}

void interrupt_enable(void) {
    ASSERT(s_interrupt_ctx.registered);

    s_interrupt_ctx.intc_fn.enable(s_interrupt_ctx.intc_ctx);

    //uint64_t daif;
    //READ_SYS_REG(DAIF, daif);
    //daif &= ~(BIT(7));
    //WRITE_SYS_REG(DAIF, daif);
}

void interrupt_disable(void) {
    ASSERT(s_interrupt_ctx.registered);

    s_interrupt_ctx.intc_fn.disable(s_interrupt_ctx.intc_ctx);

    uint64_t daif;
    READ_SYS_REG(DAIF, daif);
    daif |= BIT(7);
    WRITE_SYS_REG(DAIF, daif);
}

void interrupt_enable_irq(uint64_t irq) {
    ASSERT(s_interrupt_ctx.registered);

    s_interrupt_ctx.intc_fn.enable_irq(s_interrupt_ctx.intc_ctx, irq);
}

void interrupt_disable_irq(uint64_t irq) {
    ASSERT(s_interrupt_ctx.registered);

    s_interrupt_ctx.intc_fn.disable_irq(s_interrupt_ctx.intc_ctx, irq);
}

void interrupt_set_irq_trigger(uint64_t irq, interrupt_trigger_type_t type) {
    ASSERT(s_interrupt_ctx.registered);

    s_interrupt_ctx.intc_fn.set_irq_trigger(s_interrupt_ctx.intc_ctx, irq, type);
}

void interrupt_get_msi(uint64_t* intid_out, uint64_t* data_out, uintptr_t* addr_out) {
    ASSERT(s_interrupt_ctx.registered);

    s_interrupt_ctx.intc_fn.get_msi(s_interrupt_ctx.intc_ctx, intid_out, data_out, addr_out);
}

void interrupt_register_irq_handler(uint64_t irq, irq_handler fn, void* ctx) {
    ASSERT(s_interrupt_ctx.registered);
    ASSERT(irq < s_interrupt_ctx.num_handlers);

    uint64_t irq_state;
    BEGIN_CRITICAL(irq_state);
    s_interrupt_ctx.handlers[irq].fn = fn;
    s_interrupt_ctx.handlers[irq].ctx = ctx;
    s_interrupt_ctx.handlers[irq].intid = irq;
    END_CRITICAL(irq_state);
}

bool interrupt_await_reset(uint64_t irq) {
    ASSERT(s_interrupt_ctx.registered);
    ASSERT(irq < s_interrupt_ctx.num_handlers);

    uint64_t this_tid;
    GET_CURR_TID(this_tid);

    s_interrupt_ctx.handlers[irq].awaiter_tid = this_tid;

    return true;
}

static bool interrupt_await_wakeup(task_t* task, bool timeout, int64_t* ret) {
    ASSERT(!timeout);
    uint64_t irq = task->wait_ctx.irqnotify.irq;
    bool awaited = false;

    uint64_t irq_state;
    BEGIN_CRITICAL(irq_state);
    awaited = s_interrupt_ctx.handlers[irq].awaited;
    s_interrupt_ctx.handlers[irq].awaited = false;
    END_CRITICAL(irq_state);

    *ret = 0;
    return awaited;
}

bool interrupt_await(uint64_t irq) {

    uint64_t this_tid;
    GET_CURR_TID(this_tid);

    if (s_interrupt_ctx.handlers[irq].awaiter_tid != this_tid) {
        return false;
    }

    if (s_interrupt_ctx.handlers[irq].awaited) {
        s_interrupt_ctx.handlers[irq].awaited = false;
        s_interrupt_ctx.handlers[irq].awaiter_tid = 0;
        return true;
    }

    wait_ctx_t wait_ctx = {
        .irqnotify = {
            .irq = irq
        },
        .wake_at = 0
    };

    task_wait_kernel(get_active_task(), WAIT_IRQNOTIFY, &wait_ctx, TASK_WAIT_WAKEUP, interrupt_await_wakeup);

    return true;
}

/*
 * IRQ Context
 */
void interrupt_notify_awaiters(uint64_t irq) {
    s_interrupt_ctx.handlers[irq].awaited = true;
}

/*
 * IRQ Context
 */
void interrupt_handle_irq_entry(uint64_t vector) {
    s_interrupt_ctx.intc_fn.irq_handler(s_interrupt_ctx.intc_ctx);

}

/*
 * IRQ Context
 */
void interrupt_handle_irq(uint64_t irq) {


    if (s_interrupt_ctx.handlers[irq].fn != NULL) {
        elapsedtimer_start(&s_interrupt_ctx.handlers[irq].profile_time);
        s_interrupt_ctx.handlers[irq].fn(irq, s_interrupt_ctx.handlers[irq].ctx);
        elapsedtimer_stop(&s_interrupt_ctx.handlers[irq].profile_time);
    }

    if (s_interrupt_ctx.handlers[irq].awaiter_tid != 0) {
        interrupt_notify_awaiters(irq);
    }
}

uint64_t interrupt_get_profile_time(uint64_t idx, uint64_t* intid_out) {
    if (s_interrupt_ctx.registered) {
        *intid_out = s_interrupt_ctx.handlers[idx].intid;
        return elapsedtimer_get_us(&s_interrupt_ctx.handlers[idx].profile_time);
    } else {
        return 0;
    }
}