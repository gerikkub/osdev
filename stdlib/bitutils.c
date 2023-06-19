
#include <stdint.h>

#include "stdlib/bitutils.h"

uint16_t en_swap_16(uint16_t a) {
    uint16_t swap;
    asm ("rev16 %[out], %[in]\n" : [out] "=r" (swap) : [in] "r" (a));
    return swap;
}

uint32_t en_swap_32(uint32_t a) {
    uint32_t swap;
    asm ("rev32 %[out], %[in]\n" : [out] "=r" (swap) : [in] "r" (a));
    return swap;
}
uint64_t en_swap_64(uint64_t a) {
    uint64_t swap;
    asm ("rev64 %[out], %[in]\n" : [out] "=r" (swap) : [in] "r" (a));
    return swap;
}


uint64_t hextou64(const char* s1, uint64_t n, bool* valid) {

    uint64_t res = 0;
    bool valid_res = true;

    if (*s1 == '\0') {
        if (valid != NULL) {
            *valid = false;
        }
        return 0;
    }

    while (*s1 != '\0' && n-- && valid_res) {
        if (*s1 >= '0' &&
            *s1 <= '9') {

            res *= 16;
            res += (*s1 - '0');
        } else if (*s1 >= 'a' &&
                   *s1 <= 'f') {
            
            res *= 16;
            res += (*s1 - 'a' + 10);
        } else if (*s1 >= 'A' &&
                   *s1 <= 'F') {
            
            res *= 16;
            res += (*s1 - 'A' + 10);
        } else {
            valid_res = false;
        }
        s1++;
    }

    if (valid != NULL) {
        *valid = valid_res;
    }
    return res;
}

int64_t strtoi64(const char* str, bool* valid) {

    int64_t ret = 0;
    if (str == NULL) {
        if (valid != NULL) {
            *valid = false;
        }
        return ret;
    }

    uint64_t idx = 0;
    bool should_negate = false;
    if (str[0] == '-') {
        should_negate = true;
        idx++;
    }

    if (str[idx] > '9' ||
        str[idx] < '0') {
        
        if (valid != NULL) {
            *valid = false;
        }
        return ret;
    }

    while (str[idx] != '\0') {
        if (str[idx] > '9' ||
            str[idx] < '0') {
            break;
        }

        ret *= 10;
        ret += str[idx] - '0';

        idx++;
    }

    if (valid != NULL) {
        *valid = true;
    }
    return should_negate ? -1*ret : ret;
}