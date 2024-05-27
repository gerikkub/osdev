

#include <stdint.h>
#include <stdbool.h>

#include "kernel/gtimer.h"
#include "stdlib/bitutils.h"
#include "kernel/assert.h"
#include "kernel/console.h"

#include "kernel/interrupt/interrupt.h"

void gtimer_irq_handler(uint32_t intid, void* ctx) {

    uint32_t cntp_ctl = 0;
    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);
}

void gtimer_early_init(void) {

    // Enable the timer
    uint32_t cntp_ctl = SYS_CNTP_CTL_ENABLE; // Disable the timer

    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);
}

void gtimer_init(void) {
    interrupt_register_irq_handler(GTIMER_EL0_IRQn, gtimer_irq_handler, NULL);
    interrupt_enable_irq(GTIMER_EL0_IRQn);
}

uint64_t gtimer_get_frequency(void) {
    uint64_t freq = 0;
    READ_SYS_REG(CNTFRQ_EL0, freq);
    return freq;
}

void gtimer_start_downtimer(int32_t downcount, bool enable_interrupt) {

    ASSERT(downcount > 0);

    uint32_t cntp_ctl = 0;

    if (enable_interrupt) {
        cntp_ctl = SYS_CNTP_CTL_ENABLE;
    } else {
        cntp_ctl = SYS_CNTP_CTL_IMASK |
                   SYS_CNTP_CTL_ENABLE;
    }

    if (enable_interrupt) {
        interrupt_await_reset(GTIMER_EL0_IRQn);
    }

    // Set the downcount and enable the timer
    WRITE_SYS_REG(CNTP_TVAL_EL0, downcount);
    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);
}

void gtimer_start_downtimer_us(int32_t downcount, bool enable_interrupt) {
    int32_t ticks_per_us = gtimer_get_frequency() / (1000 * 1000);
    gtimer_start_downtimer(downcount * ticks_per_us, enable_interrupt);
}

bool gtimer_downtimer_triggered(void) {
    int32_t tval;

    READ_SYS_REG(CNTP_TVAL_EL0, tval);

    return tval <= 0;
}

uint64_t gtimer_downtimer_get_count(void) {
    volatile int32_t tval;

    READ_SYS_REG(CNTP_TVAL_EL0, tval);

    return tval;
}

void gtimer_wait_for_trigger(void) {
    interrupt_await(GTIMER_EL0_IRQn);
}

uint64_t gtimer_get_count(void) {
    volatile uint64_t cnt;

    READ_SYS_REG(CNTPCT_EL0, cnt);

    return cnt;
}

uint64_t gtimer_get_count_us(void) {
    uint64_t ticks_per_us = gtimer_get_frequency() / (1000 * 1000);
    return gtimer_get_count() / ticks_per_us;
}

void gtimer_busywait(uint32_t us) {
    uint64_t start_time_us = gtimer_get_count_us();

    volatile uint64_t end_time_us;
    do {
        end_time_us = gtimer_get_count_us();
    } while (end_time_us < (start_time_us + us));
}
