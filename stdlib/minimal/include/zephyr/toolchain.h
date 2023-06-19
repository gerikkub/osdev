
#ifndef __MINIMAL_TOOLCHAIN_H__
#define __MINIMAL_TOOLCHAIN_H__

#include <stdint.h>

typedef uint64_t size_t;

#ifndef ZRESTRICT
#define ZRESTRICT restrict
#endif

#define __printf_like(f, a)

#if !defined(__FILE_defined)
#define __FILE_defined
typedef int  FILE;
#endif

#endif
