
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/task.h"
#include "kernel/kmalloc.h"
#include "kernel/gtimer.h"
#include "kernel/interrupt/interrupt.h"

#include "drivers/spi/spi.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"


void enc28j60_discover(void* ctx) {
    //discovery_dtb_ctx_t* dtb_ctx = ctx;

    console_log(LOG_INFO, "ENC28J60 found");

    //dt_print_node(dtb_ctx->dt_ctx, dtb_ctx->dt_node, 2);
}

void enc28j60_register() {

    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "microchip,enc28j60"
        },
        .ctxfunc = enc28j60_discover
    };
    register_driver(&reg);
}

REGISTER_DRIVER(enc28j60);
