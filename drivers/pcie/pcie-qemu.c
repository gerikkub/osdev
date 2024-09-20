

#include "kernel/drivers.h"

#include "kernel/console.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"

#include "pcie.h"

void pcie_qemu_discovered(void* ctx);

static void pcie_qemu_register(void) {
    discovery_register_t dtb_register = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "pci-host-ecam-generic"
        },
        .ctxfunc = pcie_qemu_discovered
    };

    register_driver(&dtb_register);
}

REGISTER_DRIVER(pcie_qemu);

void pcie_qemu_discovered(void* ctx) {
    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    console_log(LOG_INFO, "Discovered QEMU Pcie");

    pci_ctx_t* pci_ctx = vmalloc(sizeof(pci_ctx_t));

    pci_ctx->ranges = pcie_parse_ranges(dt_node);

    pci_ctx->header_ptr = pcie_parse_allocate_reg(dt_node, &pci_ctx->header_size);


    //pci_interrupt_map_t* pcie_int_map = pcie_parse_interrupt_map(dt_node);

    pci_ctx->dtb_alloc.range_ctx = &pci_ctx->ranges;
    pci_ctx->dtb_alloc.io_top = pci_ctx->ranges.io_pci_addr.addr;
    pci_ctx->dtb_alloc.m32_top = pci_ctx->ranges.m32_pci_addr.addr;
    pci_ctx->dtb_alloc.m64_top = pci_ctx->ranges.m64_pci_addr.addr;

    //s_pcie_int_map = *pcie_int_map;
    //vfree(pcie_int_map);

    //pcie_init_interrupts();

    discover_pcie(pci_ctx);
}
