

#include <stdint.h>

#include "kernel/bitutils.h"
#include "kernel/exception.h"
#include "kernel/assert.h"

static exception_handler s_sync_handlers[NUM_EXCEPTION_EC] = {0};


exception_handler get_sync_handler(uint32_t ec) {

    ASSERT(ec < NUM_EXCEPTION_EC);

    return s_sync_handlers[ec];
}

void set_sync_handler(uint32_t ec, exception_handler handler) {

    ASSERT(ec < NUM_EXCEPTION_EC);
    ASSERT(s_sync_handlers[ec] == NULL);
    ASSERT(handler != NULL);

    s_sync_handlers[ec] = handler;
}
