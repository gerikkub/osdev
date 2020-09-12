#ifndef __ASSERT_H__
#define __ASSERT_H__

#include "kernel/panic.h"

#define ASSERT(X) \
if (!(X)) { \
    panic(__FILE__, __LINE__, #X); \
}

// #define ASSERT_ARGS(X, ...) \
// if (!(X)) { \
//     panic("Panic: ## __FILE__ ## : ## __LINE__ ## #X",  __VA_ARGS__ ); \
// }

#endif