
#include "kernel/lib/vmalloc.h"

#include "stdlib/bitutils.h"
#include "stdlib/bitalloc.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/lib/libdtb.h"

#include "drivers/timer/bcm2835_systemtimer.h"

#define COUNTER (1)

typedef struct {
    volatile BCM2835SystemTimer_t* timer;
    dt_node_t* dt_node;
    uint64_t intids[4];
} bcm2835_sys_timer_ctx_t;

static void* bcm2835_ctx;

void* bcm2835_get_ctx(void) {
    return bcm2835_ctx;
}

void bcm2835_systemtimer_irq_handler(uint32_t intid, void* ctx) {
    bcm2835_sys_timer_ctx_t* timer_ctx = ctx;
    timer_ctx->timer->cs = 1 << COUNTER;

    console_log(LOG_DEBUG, "BCM2835 Timer IRQ");
}

bool bcm2835_systemtimer_pending(void* ctx) {
    bcm2835_sys_timer_ctx_t* timer_ctx = ctx;

    return (timer_ctx->timer->cs & (1 << COUNTER)) != 0;
}

uint32_t bcm2835_systemtimer_count(void* ctx) {
    bcm2835_sys_timer_ctx_t* timer_ctx = ctx;

    return timer_ctx->timer->clo;
}

void bcm2835_systemtimer_trigger_irq(void* ctx, uint64_t us) {
    bcm2835_sys_timer_ctx_t* timer_ctx = ctx;

    interrupt_await_reset(timer_ctx->intids[COUNTER]);

    timer_ctx->timer->cs = 0xF;

    uint32_t timer_val = timer_ctx->timer->clo;

    uint32_t freq = 0xf4240;

    uint32_t count = (freq * us) / (1000 * 1000);
    uint32_t trigger = timer_val + count;

    timer_ctx->timer->c[COUNTER] = trigger;

    console_log(LOG_DEBUG, "ST Trigger at %d", trigger);
}

void bcm2835_irq_reg(void* ctx) {

    bcm2835_sys_timer_ctx_t* timer_ctx = ctx;

    ASSERT(timer_ctx->dt_node->prop_ints->handler);
    ASSERT(timer_ctx->dt_node->prop_ints->handler->dtb_funcs);

    dt_node_t* intc_node = timer_ctx->dt_node->prop_ints->handler;
    intc_dtb_funcs_t* intc_dtb_funcs = intc_node->dtb_funcs;

    intc_dtb_funcs->get_intid_list(intc_node->dtb_ctx,
                                   timer_ctx->dt_node->prop_ints,
                                   &timer_ctx->intids[0]);

    intc_dtb_funcs->setup_intids(intc_node->dtb_ctx,
                                 timer_ctx->dt_node->prop_ints);

    console_log(LOG_INFO, "BCM2835 intids %d %d %d %d",
                timer_ctx->intids[0], timer_ctx->intids[1],
                timer_ctx->intids[2], timer_ctx->intids[3]);

    interrupt_register_irq_handler(timer_ctx->intids[0], bcm2835_systemtimer_irq_handler, ctx);
    interrupt_register_irq_handler(timer_ctx->intids[1], bcm2835_systemtimer_irq_handler, ctx);
    interrupt_register_irq_handler(timer_ctx->intids[2], bcm2835_systemtimer_irq_handler, ctx);
    interrupt_register_irq_handler(timer_ctx->intids[3], bcm2835_systemtimer_irq_handler, ctx);
    // interrupt_enable_irq(timer_ctx->intids[0]);
    interrupt_enable_irq(timer_ctx->intids[1]);
    // interrupt_enable_irq(timer_ctx->intids[2]);
    interrupt_enable_irq(timer_ctx->intids[3]);
}

void bcm2835_systemtimer_discover(void* ctx) {

    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    ASSERT(dt_node->prop_reg);
    ASSERT(dt_node->prop_ints);
    ASSERT(dt_node->prop_ints->num_ints == 4);

    bcm2835_sys_timer_ctx_t* timer_ctx = vmalloc(sizeof(bcm2835_sys_timer_ctx_t));
    timer_ctx->dt_node = dt_node;

    dt_prop_reg_entry_t* reg_entries = dt_node->prop_reg->reg_entries;

    uintptr_t addr_bus;
    if (reg_entries->addr_size == 1) {
        addr_bus = reg_entries->addr_ptr[0];
    } else {
        addr_bus = ((uint64_t)reg_entries->addr_ptr[0] << 32) |
                    (uint64_t)reg_entries->addr_ptr[1];
    }

    bool addr_valid;
    uintptr_t timer_ctx_phy = dt_map_addr_to_phy(dt_node, addr_bus, &addr_valid);

    ASSERT(addr_valid);

    console_log(LOG_DEBUG, "SystemTimer @ %16x", timer_ctx_phy);

    //uintptr_t timer_ctx_phy = (0x47c000000 + 0x02003000);
    timer_ctx->timer = PHY_TO_KSPACE_PTR(timer_ctx_phy);

    memspace_map_device_kernel((void*)timer_ctx_phy,
                               (void*)timer_ctx->timer,
                               VMEM_PAGE_SIZE,
                               MEMSPACE_FLAG_PERM_KRW);
    memspace_update_kernel_vmem();

    timer_ctx->timer->cs = 0xF;
    timer_ctx->timer->c[0] = 0;
    timer_ctx->timer->c[1] = 0;
    timer_ctx->timer->c[2] = 0;
    timer_ctx->timer->c[3] = 0;

    driver_register_late_init(bcm2835_irq_reg, timer_ctx);

    bcm2835_ctx = timer_ctx;
}

void bcm2835_systemtimer_register() {
    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "brcm,bcm2835-system-timer"
        },
        .ctxfunc = bcm2835_systemtimer_discover
    };
    register_driver(&reg);

}

REGISTER_DRIVER(bcm2835_systemtimer)