
#include <stdint.h>
#include <string.h>

#include "kernel/drivers.h"

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libpci.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/lib/utils.h"

typedef struct {
    pci_device_ctx_t* pci_ctx;
    void* bar_vmem;
} xhci_ctx_t;

static void xhci_probe(void* ctx) {
    
    xhci_ctx_t* xhci = vmalloc(sizeof(xhci_ctx_t));

    xhci->pci_ctx = ctx;
    xhci->bar_vmem = xhci->pci_ctx->bar[0].vmem;

    uint16_t hciver = READ_MEM16(xhci->bar_vmem + 2);
    console_log(LOG_INFO, "Discovered XHCI (%4x)", hciver);
}

static void xhci_discover_device(void* ctx) {

    driver_register_late_init(xhci_probe, ctx);
}

static discovery_register_t s_xhci_register = {
    .type = DRIVER_DISCOVERY_PCI,
    .pci = {
        .vendor_id = 0x1106,
        .device_id = 0x3483
    },
    .ctxfunc = xhci_probe
};

void xhci_register(void) {

    register_driver(&s_xhci_register);
}

REGISTER_DRIVER(xhci)

