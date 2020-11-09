
#ifndef __GTIMER_H__
#define __GTIMER_H__

#include <stdint.h>

#define GTIMER_FREQ (1000 * 1000)


#define SYS_CNTP_CTL_ISTATUS BIT(2)
#define SYS_CNTP_CTL_IMASK   BIT(1)
#define SYS_CNTP_CTL_ENABLE BIT(0)

void gtimer_init(void);

void gtimer_start_downtimer(int32_t downcount, bool enable_interrupt);

bool gtimer_downtimer_triggered(void);

uint64_t gtimer_get_count(void);


#endif