

#include <stdint.h>

#include "stdlib/bitutils.h"
#include "kernel/exception.h"
#include "kernel/assert.h"

static sync_handler s_sync_handlers[NUM_EXCEPTION_EC] = {0};


sync_handler get_sync_handler(uint32_t ec) {

    ASSERT(ec < NUM_EXCEPTION_EC);

    return s_sync_handlers[ec];
}

void set_sync_handler(uint32_t ec, sync_handler handler) {

    ASSERT(ec < NUM_EXCEPTION_EC);
    ASSERT(s_sync_handlers[ec] == NULL);
    ASSERT(handler != NULL);

    s_sync_handlers[ec] = handler;
}
