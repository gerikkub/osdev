
#ifndef __GTIMER_H__
#define __GTIMER_H__

#include <stdint.h>
#include <stdbool.h>

#define GTIMER_FREQ (1000 * 1000)

#define GTIMER_EL0_IRQn (16 + 14)

#define SYS_CNTP_CTL_ISTATUS BIT(2)
#define SYS_CNTP_CTL_IMASK   BIT(1)
#define SYS_CNTP_CTL_ENABLE BIT(0)

void gtimer_early_init(void);
void gtimer_init(void);
void gtimer_enable_irq(void);

uint64_t gtimer_get_frequency();
void gtimer_start_downtimer(int32_t downcount, bool enable_interrupt);
void gtimer_start_downtimer_us(int32_t downcount_us, bool enable_interrupt);

bool gtimer_downtimer_triggered(void);
uint64_t gtimer_downtimer_get_count(void);
void gtimer_wait_for_trigger(void);

uint64_t gtimer_get_count(void);
uint64_t gtimer_get_count_us(void);


#endif
