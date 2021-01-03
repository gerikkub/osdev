
#ifndef __SYSTEM_ASSERT_H__
#define __SYSTEM_ASSERT_H__

#include "system/lib/system_console.h"
#include "stdlib/printf.h"

#define SYS_ASSERT(X)  \
if (!(X)) { \
    console_printf("panic: %s:%u " #X "\n", __FILE__, __LINE__); \
    console_flush(); \
    while (1); \
}

#endif