
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/assert.h"
#include "stdlib/bitutils.h"

void * memcpy(void *restrict dst, const void *restrict src, size_t n) {
    uint8_t *restrict dst_8 = dst;
    const uint8_t *restrict src_8 = src;

    for (size_t i = 0; i < n; i++) {
        dst_8[i] = src_8[i];
    }
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const int8_t* s1_8 = s1;
    const int8_t* s2_8 = s2;

    for (size_t i = 0; i < n; i++) {
        if (s1_8[i] != s2_8[i]) {
            if (s1_8[i] < s2_8[i]) {
                return -1;
            } else {
                return 1;
            }
        }
    }

    return 0;
}

void* memset(void* b, int c, size_t len) {
    uint8_t c_8 = c;
    uint8_t* b_8 = b;

    while (len--) {
        *b_8++ = c_8;
    }

    return b;
}

size_t strlen(const char *s) {
    uint64_t count = 0;
    while (*s != '\0') {
        s++;
        count++;
    }
    return count;
}

size_t strnlen(const char *s, size_t maxlen) {
    uint64_t count = 0;
    while (*s != '\0') {
        s++;
        count++;
        if (maxlen == count) {
            break;
        }
    }
    return count;
}

char * strcpy(char * dst, const char * src) {
    // Should never use strcpy!!!
    while (1);
    (void)dst;
    (void)src;
    return NULL;
}

char * strncpy(char * dst, const char * src, size_t len) {
    size_t count = 0;
    char* dst_orig = dst;
    while (*src != '\0' &&
           count < len) {
        *dst = *src;
        dst++;
        src++;
        count++;
    }

    while (count < len) {
        *dst = '\0';
        count++;
    }

    return dst_orig;
}

int strcmp(const char *s1, const char *s2) {
    // Should never call!
    while(1);
    (void)s1;
    (void)s2;
    return 0;
}
int strncmp(const char *s1, const char *s2, size_t n) {
    size_t count = 0;;
    while (*s1 != '\0' &&
           *s2 != '\0' &&
           *s1 == *s2 &&
           count < n) {

        s1++;
        s2++;
        count++;
    }

    if (count == n ||
        (*s1 == '\0' &&
         *s2 == '\0')) {
        return 0;
    }

    uint8_t* u1 = (uint8_t*)s1;
    uint8_t* u2 = (uint8_t*)s2;

    if ((*u1) > (*u2)) {
        return 1;
    } else {
        return -1;
    }
}

uint64_t hextou64(const char* s1, size_t n, bool* valid) {

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