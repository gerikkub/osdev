

#include <stdint.h>
#include <stdbool.h>

#include "kernel/gtimer.h"
#include "kernel/bitutils.h"
#include "kernel/assert.h"



void gtimer_init(void) {

    // Setup frequency

    // Enable the timer
    uint32_t cntp_ctl = SYS_CNTP_CTL_IMASK | // Mask interrupts for now
                        SYS_CNTP_CTL_ENABLE; // Enable the timer

    WRITE_SYS_REG(CNTP_CTL_EL0, cntp_ctl);
}

void gtimer_start_downtimer(int32_t downcount) {

    ASSERT(downcount > 0);

    WRITE_SYS_REG(CNTP_TVAL_EL0, downcount);
}

bool gtimer_downtimer_triggered(void) {
    int32_t tval;

    READ_SYS_REG(CNTP_TVAL_EL0, tval);

    return tval <= 0;
}

uint64_t gtimer_get_count(void) {
    uint64_t cnt;

    READ_SYS_REG(CNTPCT_EL0, cnt);

    return cnt;
}

