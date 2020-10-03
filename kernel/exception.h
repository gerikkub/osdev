#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <stdint.h>

#include "kernel/bitutils.h"

#define NUM_EXCEPTION_EC (BITSIZE(9))

typedef void (*exception_handler)(uint64_t vector);
typedef void (*sync_handler)(uint64_t vector, uint32_t esr);

sync_handler get_sync_handler(uint32_t ec);
void set_sync_handler(uint32_t ec, sync_handler handler);

void exception_init();

#define EC_SVC (0x15)
#define EC_INST_ABORT_LOWER (0x20)
#define EC_INST_ABORT (0x21)
#define EC_DATA_ABORT_LOWER (0x24)
#define EC_DATA_ABORT (0x25)

#define VECTOR_FROM_CURR_EL_SP0(x) ((x & 0xC) == 0)
#define VECTOR_FROM_CURR_EL_SPx(x) ((x & 0xC) == 0x4)
#define VECTOR_FROM_LOW_64(x) ((x & 0xC) == 0x8)
#define VECTOR_FROM_LOW_32(x) ((x & 0xC) == 0xC)

#define VECTOR_IS_SYNC(x) ((x & 0x3) == 0)
#define VECTOR_IS_IRQ(x) ((x & 0x3) == 0x1)
#define VECTOR_IS_FIQ(x) ((x & 0x3) == 0x2)
#define VECTOR_IS_SERROR(x) ((x & 0x3) == 0x3)


#endif
