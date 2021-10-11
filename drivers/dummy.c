

#include "kernel/drivers.h"
#include "kernel/assert.h"

#include "kernel/console.h"

#define TARGET_HAVE_CTORS_DTORS 1

void dummy_register(void) {

    console_printf("Dummy Init\n");
}

REGISTER_DRIVER(dummy);
