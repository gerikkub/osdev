
#ifndef __LIB_STRING_H__
#define __LIB_STRING_H__

#include <stdint.h>

typedef uint64_t size_t;

void * memcpy(void *restrict dst, const void *restrict src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char * strcpy(char * dst, const char * src);
char * strncpy(char * dst, const char * src, size_t len);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

#endif