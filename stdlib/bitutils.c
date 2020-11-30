
#include <stdint.h>

#include "stdlib/bitutils.h"

uint16_t en_swap_16(uint16_t a) {
    uint16_t swap;
    asm ("rev16 %[out], %[in]\n" : [out] "=r" (swap) : [in] "r" (a));
    return swap;
}

uint32_t en_swap_32(uint32_t a) {
    uint16_t swap;
    asm ("rev32 %[out], %[in]\n" : [out] "=r" (swap) : [in] "r" (a));
    return swap;
}
uint64_t en_swap_64(uint64_t a) {
    uint16_t swap;
    asm ("rev64 %[out], %[in]\n" : [out] "=r" (swap) : [in] "r" (a));
    return swap;
}