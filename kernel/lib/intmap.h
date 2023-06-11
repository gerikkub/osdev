
#ifndef __INTMAP_H__
#define __INTMAP_H__

#include <stdint.h>

#include "kernel/lib/hashmap.h"

hashmap_ctx_t* uintmap_alloc(uint64_t init_log_len);

#endif
