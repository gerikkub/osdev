
#ifndef __LIB_ELAPSEDTIMER_H__
#define __LIB_ELAPSEDTIMER_H__

#include <stdint.h>

typedef struct {
    uint64_t elapsed;

    uint64_t start_time;
} elapsedtimer_t;

void elapsedtimer_clear(elapsedtimer_t* t);
void elapsedtimer_start(elapsedtimer_t* t);
void elapsedtimer_stop(elapsedtimer_t* t);
uint64_t elapsedtimer_get_us(elapsedtimer_t* t);



#endif