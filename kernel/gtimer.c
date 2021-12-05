

#include <stdint.h>
#include <stdbool.h>

#include "kernel/gtimer.h"
#include "stdlib/bitutils.h"
#include "kernel/assert.h"

#include "kernel/interrupt/interrupt.h"

void gtimer_irq_handler(uint32_t intid, void* ctx) {

    uint32_t cntp_ctl = 0;
    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);
}


void gtimer_init(void) {

    // Setup frequency

    // Enable the timer
    uint32_t cntp_ctl = 0; // Disable the timer

    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);

    interrupt_register_irq_handler(GTIMER_EL0_IRQn, gtimer_irq_handler, NULL);
}

uint64_t gtimer_get_frequency(void) {
    uint64_t freq = 0;
    READ_SYS_REG(CNTFRQ_EL0, freq);
    return freq;
}

void gtimer_start_downtimer(int32_t downcount, bool enable_interrupt) {

    ASSERT(downcount > 0);

    // Disable timer output
    uint32_t cntp_ctl = 0;
    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);

    if (enable_interrupt) {
        cntp_ctl = SYS_CNTP_CTL_ENABLE;
    } else {
        cntp_ctl = SYS_CNTP_CTL_IMASK |
                   SYS_CNTP_CTL_ENABLE;
    }

    if (enable_interrupt) {
        interrupt_enable_irq(GTIMER_EL0_IRQn);
        interrupt_await_reset(GTIMER_EL0_IRQn);
    }

    // Set the downcount and enable the timer
    WRITE_SYS_REG(CNTP_TVAL_EL0, downcount);
    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);
}

bool gtimer_downtimer_triggered(void) {
    int32_t tval;

    READ_SYS_REG(CNTP_TVAL_EL0, tval);

    return tval <= 0;
}

void gtimer_wait_for_trigger(void) {
    interrupt_await(GTIMER_EL0_IRQn);
}

uint64_t gtimer_get_count(void) {
    uint64_t cnt;

    READ_SYS_REG(CNTPCT_EL0, cnt);

    return cnt;
}

