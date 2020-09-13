#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <stdint.h>

#include "kernel/bitutils.h"

#define NUM_EXCEPTION_EC (BITSIZE(9))

typedef void (*exception_handler)(uint32_t vector, uint32_t esr);

exception_handler get_sync_handler(uint32_t ec);
void set_sync_handler(uint32_t ec, exception_handler);

void exception_init();

#define EC_INST_ABORT_LOWER (0x20)
#define EC_INST_ABORT (0x21)
#define EC_DATA_ABORT_LOWER (0x24)
#define EC_DATA_ABORT (0x25)

#endif
