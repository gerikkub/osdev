
#include <stdint.h>

#include "kernel/gtimer.h"

#include "kernel/lib/elapsedtimer.h"

void elapsedtimer_clear(elapsedtimer_t* t) {
    t->elapsed = 0;
    t->start_time = 0;
}

void elapsedtimer_start(elapsedtimer_t* t) {
    t->start_time = gtimer_get_count();
}

void elapsedtimer_stop(elapsedtimer_t* t) {
    if (t->start_time != 0) {
        t->elapsed += gtimer_get_count() - t->start_time;
        t->start_time = 0;
    }
}

uint64_t elapsedtimer_get_us(elapsedtimer_t* t) {
    uint64_t elapsed = t->elapsed;
    if (t->start_time != 0) {
        elapsed += gtimer_get_count() - t->start_time;
    }
    return elapsed / (gtimer_get_frequency() / (1000 * 1000));
}