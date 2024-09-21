
#include <string.h>

#include "kernel/drivers.h"

#include "kernel/console.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/utils.h"

#include "pcie.h"

void pcie_qemu_discovered(void* ctx);

typedef struct {
    void* base_ptr;
    int64_t base_size;
} pci_qemu_ctx_t;

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

static void pcie_qemu_cntr_read_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, void* header_ptr) {
    ASSERT(header_offset % 4096 == 0);
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    memcpy(header_ptr, pci_qemu_ctx->base_ptr + header_offset, 4096);
}

static uint8_t pcie_qemu_cntr_read8_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint8_t)));
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    return READ_MEM8(pci_qemu_ctx->base_ptr + header_offset + offset);
}

static uint16_t pcie_qemu_cntr_read16_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint16_t)));
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    return READ_MEM16(pci_qemu_ctx->base_ptr + header_offset + offset);
}

static uint32_t pcie_qemu_cntr_read32_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint32_t)));
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    return READ_MEM32(pci_qemu_ctx->base_ptr + header_offset + offset);
}

static void pcie_qemu_cntr_write8_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset, uint8_t data) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint8_t)));
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    WRITE_MEM8(pci_qemu_ctx->base_ptr + header_offset + offset, data);
}

static void pcie_qemu_cntr_write16_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset, uint16_t data) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint16_t)));
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    WRITE_MEM16(pci_qemu_ctx->base_ptr + header_offset + offset, data);
}

static void pcie_qemu_cntr_write32_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset, uint32_t data) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint32_t)));
    pci_qemu_ctx_t* pci_qemu_ctx = pci_ctx->cntr_ctx;;
    WRITE_MEM32(pci_qemu_ctx->base_ptr + header_offset + offset, data);
}

static pci_cntr_ops_t qemu_header_ops = {
    .read = pcie_qemu_cntr_read_header_fn,
    .read8 = pcie_qemu_cntr_read8_header_fn,
    .read16 = pcie_qemu_cntr_read16_header_fn,
    .read32 = pcie_qemu_cntr_read32_header_fn,
    .write8 = pcie_qemu_cntr_write8_header_fn,
    .write16 = pcie_qemu_cntr_write16_header_fn,
    .write32 = pcie_qemu_cntr_write32_header_fn
};

void pcie_qemu_discovered(void* ctx) {
    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    console_log(LOG_INFO, "Discovered QEMU Pcie");

    pci_ctx_t* pci_ctx = vmalloc(sizeof(pci_ctx_t));
    pci_qemu_ctx_t* pci_qemu_ctx = vmalloc(sizeof(pci_qemu_ctx_t));

    pci_ctx->cntr_ctx = pci_qemu_ctx;
    pci_ctx->ranges = pcie_parse_ranges(dt_node);

    pci_qemu_ctx->base_ptr = pcie_parse_allocate_reg(dt_node, &pci_qemu_ctx->base_size);
    pci_ctx->header_ops = qemu_header_ops;

    //pci_interrupt_map_t* pcie_int_map = pcie_parse_interrupt_map(dt_node);

    pci_ctx->dtb_alloc.range_ctx = &pci_ctx->ranges;
    pci_ctx->dtb_alloc.io_top = pci_ctx->ranges.io_pci_addr.addr;
    pci_ctx->dtb_alloc.m32_top = pci_ctx->ranges.m32_pci_addr.addr;
    pci_ctx->dtb_alloc.m64_top = pci_ctx->ranges.m64_pci_addr.addr;

    //s_pcie_int_map = *pcie_int_map;
    //vfree(pcie_int_map);

    //pcie_init_interrupts();

    discover_pcie(pci_ctx, 256*32*8);
}
