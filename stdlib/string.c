
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/bitutils.h"

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

    return (*u1) > (*u2);
}