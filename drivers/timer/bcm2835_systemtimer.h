#ifndef __TIMER_BCM2835_SYSTEMTIMER_H__
#define __TIMER_BCM2835_SYSTEMTIMER_H__

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t cs;
    uint32_t clo;
    uint32_t chi;
    uint32_t c[4];
} BCM2835SystemTimer_t;

void* bcm2835_get_ctx(void);

bool bcm2835_systemtimer_pending(void* ctx);
uint32_t bcm2835_systemtimer_count(void* ctx);
void bcm2835_systemtimer_trigger_irq(void* ctx, uint64_t us);


#endif