
#ifndef __MINIMAL_LIBC_HOOKS_H__
#define __MINIMAL_LIBC_HOOKS_H__

#include <zephyr/toolchain.h>

int zephyr_fputc(int c, FILE * stream);

size_t zephyr_fwrite(const void *ZRESTRICT ptr, size_t size,
				size_t nitems, FILE *ZRESTRICT stream);

#endif
