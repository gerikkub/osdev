
#include <stdint.h>
#include <stdbool.h>

#include "kernel/gtimer.h"

uint64_t time_get_bootns(void) {

    uint64_t freq = gtimer_get_frequency();

    uint64_t ns_per_tick = (1000*1000*1000) / freq;

    uint64_t count = gtimer_get_count();

    return count * ns_per_tick;
}