#ifndef __ASSERT_H__
#define __ASSERT_H__

#include "kernel/panic.h"

#define ASSERT(X) \
if (!(X)) { \
    panic(__FILE__, __LINE__, #X); \
}

#endif
