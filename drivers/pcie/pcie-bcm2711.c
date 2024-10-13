

#include "kernel/drivers.h"
#include "kernel/console.h"
#include "kernel/gtimer.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/utils.h"
#include "stdlib/printf.h"

#include "stdlib/bitutils.h"

#include "pcie.h"

#define REG_CONTROLLER_HW_VER 0x406c
#define REG_BRIDGE_CTRL 0x9210
#define BRIDGE_CTRL_DISABLE 0x1
#define BRIDGE_CTRL_RESET 0x2
#define REG_PCIE_HARD_DEBUG 0x4204
#define REG_BRIDGE_STATE 0x4068
#define REG_BRIDGE_LINK_STATE 0x00bc
#define REG_BUS_WINDOW_LOW 0x400c
#define REG_BUS_WINDOW_HIGH 0x4010
#define REG_CPU_WINDOW_LOW 0x4070
#define REG_CPU_WINDOW_START_HIGH 0x4080
#define REG_CPU_WINDOW_END_HIGH 0x4084
#define REG_INTMASK 0x4510
#define REG_INTCLR 0x4514

#define REG_PCI_ID_VAL 0x43c

#define REG_CFG_WINDOW 0x9000
#define REG_CFG_DATA   0x8000

void pcie_bcm2711_discovered(void* ctx);

typedef struct {
    void* cfg_ptr;
    int64_t cfg_size;
} pci_bcm2711_ctx_t;

static void pcie_bcm2711_register(void) {
    discovery_register_t dtb_register = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "brcm,bcm2711-pcie"
        },
        .ctxfunc = pcie_bcm2711_discovered
    };

    register_driver(&dtb_register);
}

REGISTER_DRIVER(pcie_bcm2711);

static void pcie_bcm2711_select_cfg(pci_bcm2711_ctx_t* bcm_ctx, uint64_t header_offset, uint64_t offset) {

    //console_log(LOG_DEBUG, "Selecting PCI window %8x", header_offset);
    WRITE_MEM32(bcm_ctx->cfg_ptr + REG_CFG_WINDOW, header_offset);
    //console_log(LOG_DEBUG, "Done");
    MEM_DMB();
    //console_log(LOG_DEBUG, "Done2");
}

static uint8_t pcie_bcm2711_cntr_read8_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint8_t)));
    //console_log(LOG_DEBUG, "PCI Read8 %8x %4x", header_offset, offset);
    if (header_offset < 4096) {
        return READ_MEM8(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + offset);
    } else if (header_offset < (1 << 20)) {
        return 0xFF;
    } else {
        pcie_bcm2711_select_cfg(pci_ctx->cntr_ctx, header_offset, offset);
        //console_log(LOG_DEBUG, "Done8 3 %8x %16x",
                    //REG_CFG_DATA + offset,
                    //((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset);
        return READ_MEM8(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset);
    }
}

static uint16_t pcie_bcm2711_cntr_read16_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint16_t)));
    //console_log(LOG_DEBUG, "PCI Read16 %8x %4x", header_offset, offset);
    if (header_offset < 4096) {
        return READ_MEM16(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + offset);
    } else if (header_offset < (1 << 20)) {
        return 0xFF;
    } else {
        pcie_bcm2711_select_cfg(pci_ctx->cntr_ctx, header_offset, offset);
        //console_log(LOG_DEBUG, "Done16 3 %8x %16x",
                    //REG_CFG_DATA + offset,
                    //((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset);
        uint16_t val = READ_MEM16(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset);
        //console_log(LOG_DEBUG, "Done16 4");
        return val;
    }
}

static uint32_t pcie_bcm2711_cntr_read32_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint32_t)));
    //console_log(LOG_DEBUG, "PCI Read32 %8x %4x", header_offset, offset);
    if (header_offset < 4096) {
        return READ_MEM32(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + offset);
    } else if (header_offset < (1 << 20)) {
        return 0xFF;
    } else {
        pcie_bcm2711_select_cfg(pci_ctx->cntr_ctx, header_offset, offset);
        return READ_MEM32(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset);
    }
}

static void pcie_bcm2711_cntr_write8_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset, uint8_t data) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint8_t)));
    //console_log(LOG_DEBUG, "PCI Write8 %8x %4x", header_offset, offset);
    if (header_offset < 4096) {
        WRITE_MEM8(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + offset, data);
    } else if (header_offset < (1 << 20)) {
        return;
    } else {
        pcie_bcm2711_select_cfg(pci_ctx->cntr_ctx, header_offset, offset);
        WRITE_MEM8(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset, data);
    }
    MEM_DMB();
}

static void pcie_bcm2711_cntr_write16_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset, uint16_t data) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint16_t)));
    //console_log(LOG_DEBUG, "PCI Write16 %8x %4x", header_offset, offset);
    if (header_offset < 4096) {
        WRITE_MEM16(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + offset, data);
    } else if (header_offset < (1 << 20)) {
        return;
    } else {
        pcie_bcm2711_select_cfg(pci_ctx->cntr_ctx, header_offset, offset);
        WRITE_MEM16(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset, data);
    }
    MEM_DMB();
}

static void pcie_bcm2711_cntr_write32_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, uint64_t offset, uint32_t data) {
    ASSERT(header_offset % 4096 == 0);
    ASSERT(offset <= (4096 - sizeof(uint32_t)));
    //console_log(LOG_DEBUG, "PCI Write32 %8x %4x", header_offset, offset);
    if (header_offset < 4096) {
        WRITE_MEM32(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + offset, data);
    } else if (header_offset < (1 << 20)) {
        return;
    } else {
        pcie_bcm2711_select_cfg(pci_ctx->cntr_ctx, header_offset, offset);
        WRITE_MEM32(((pci_bcm2711_ctx_t*)pci_ctx->cntr_ctx)->cfg_ptr + REG_CFG_DATA + offset, data);
    }
    MEM_DMB();
}

static void pcie_bcm2711_cntr_read_header_fn(pci_ctx_t* pci_ctx, uint64_t header_offset, void* header_ptr) {
    ASSERT(header_offset % 4096 == 0);

    uint32_t* header_ptr_u32 = header_ptr;
    for (int32_t idx = 0; idx < 4096/4; idx++) {
        header_ptr_u32[idx] = pcie_bcm2711_cntr_read32_header_fn(pci_ctx, header_offset, idx*4);
    }
}

static pci_cntr_ops_t bcm2711_header_ops = {
    .read = pcie_bcm2711_cntr_read_header_fn,
    .read8 = pcie_bcm2711_cntr_read8_header_fn,
    .read16 = pcie_bcm2711_cntr_read16_header_fn,
    .read32 = pcie_bcm2711_cntr_read32_header_fn,
    .write8 = pcie_bcm2711_cntr_write8_header_fn,
    .write16 = pcie_bcm2711_cntr_write16_header_fn,
    .write32 = pcie_bcm2711_cntr_write32_header_fn
};

void pcie_bcm2711_discovered(void* ctx) {
    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;


    pci_ctx_t* pci_ctx = vmalloc(sizeof(pci_ctx_t));
    pci_bcm2711_ctx_t* pci_bcm2711_ctx = vmalloc(sizeof(pci_bcm2711_ctx_t));

    pci_ctx->cntr_ctx = pci_bcm2711_ctx;
    pci_ctx->ranges = pcie_parse_ranges(dt_node);

    pci_bcm2711_ctx->cfg_ptr = pcie_parse_allocate_reg(dt_node, &pci_bcm2711_ctx->cfg_size);
    pci_ctx->header_ops = bcm2711_header_ops;

    console_log(LOG_INFO, "Discovered Pcie @ %16x", pci_bcm2711_ctx->cfg_ptr);

    // Device tree lists memory as 32-bit, but XHCI requests 64-bit bars
    pci_ctx->ranges.m64_pci_addr = pci_ctx->ranges.m32_pci_addr;
    pci_ctx->ranges.m64_pci_addr.space = PCI_SPACE_M64;
    pci_ctx->ranges.m64_mem_addr = pci_ctx->ranges.m32_mem_addr;
    pci_ctx->ranges.m64_size = pci_ctx->ranges.m32_size;
    pci_ctx->ranges.m32_mem_addr = 0;
    pci_ctx->ranges.m32_size = 0;

    pci_ctx->dtb_alloc.range_ctx = &pci_ctx->ranges;
    pci_ctx->dtb_alloc.io_top = pci_ctx->ranges.io_pci_addr.addr;
    pci_ctx->dtb_alloc.m32_top = pci_ctx->ranges.m32_pci_addr.addr;
    pci_ctx->dtb_alloc.m64_top = pci_ctx->ranges.m64_pci_addr.addr;

    console_log(LOG_DEBUG, "PCIe IO Ranges %16x %16x %16x",
                pci_ctx->ranges.io_pci_addr.addr,
                pci_ctx->ranges.io_mem_addr,
                pci_ctx->ranges.io_size);

    console_log(LOG_DEBUG, "PCIe M32 Ranges %16x %16x %16x",
                pci_ctx->ranges.m32_pci_addr.addr,
                pci_ctx->ranges.m32_mem_addr,
                pci_ctx->ranges.m32_size);

    console_log(LOG_DEBUG, "PCIe M64 Ranges %16x %16x %16x",
                pci_ctx->ranges.m64_pci_addr.addr,
                pci_ctx->ranges.m64_mem_addr,
                pci_ctx->ranges.m64_size);

    // Reset
    uint32_t bridge_ctrl = READ_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_CTRL);
    bridge_ctrl |= BRIDGE_CTRL_DISABLE | BRIDGE_CTRL_RESET;
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_CTRL, bridge_ctrl);
    MEM_DMB();

    gtimer_busywait(100);


    bridge_ctrl = READ_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_CTRL);
    bridge_ctrl &= ~(BRIDGE_CTRL_RESET);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_CTRL, bridge_ctrl);
    MEM_DMB();

    gtimer_busywait(100);

    // Clear and mask interrupts
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_INTMASK, 0xFFFFFFFF);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_INTCLR, 0xFFFFFFFF);


    bridge_ctrl = READ_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_CTRL);
    bridge_ctrl &= ~(BRIDGE_CTRL_DISABLE);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_CTRL, bridge_ctrl);
    MEM_DMB();

    gtimer_busywait(100);

    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_PCIE_HARD_DEBUG, 0);
    MEM_DMB();

    gtimer_busywait(100);


    int tries;
    uint32_t status = READ_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_STATE);
    for (tries = 1000; tries > 0; tries--) {
        if ((status & 0x30) == 0x30) {
            break;
        }
        gtimer_busywait(100);
        status = READ_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BRIDGE_STATE);
    }
    if (tries == 0) {
        console_log(LOG_INFO, "Unable to initialize PCIe controller (%2x)", status);
        return;
    }

    if ((status & 0x80) != 0x80) {
        console_log(LOG_INFO, "PCIe not in RC mode (%2x)", status);
        return;
    }

    uint32_t hw_version = READ_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_CONTROLLER_HW_VER);
    console_log(LOG_INFO, "PCIe HW VER: %4x", hw_version & 0xFFFF);

    // Forward CPU and PCI memory mappings to PCI
    console_log(LOG_INFO, "BUS Window: %16x", pci_ctx->ranges.m64_pci_addr.addr);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BUS_WINDOW_LOW,
                pci_ctx->ranges.m64_pci_addr.addr & 0xFFFFFFFF);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_BUS_WINDOW_HIGH,
                (pci_ctx->ranges.m64_pci_addr.addr >> 32) & 0xFFFFFFFF);

    uint64_t m64_start = (uint64_t)dt_node->prop_ranges->range_entries[0].paddr_ptr[1] |
                         ((uint64_t)dt_node->prop_ranges->range_entries[0].paddr_ptr[0] << 32);
    uint64_t m64_size = (uint64_t)dt_node->prop_ranges->range_entries[0].size_ptr[1] |
                         ((uint64_t)dt_node->prop_ranges->range_entries[0].size_ptr[0] << 32);
    uint64_t m64_end = m64_start + m64_size;
    console_log(LOG_INFO, "CPU Window: [%16x:%16x]", m64_start, m64_end);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_CPU_WINDOW_LOW,
                ((m64_start >> 16) & 0xFFF0) |
                 ((m64_end - 1) & 0xFFF00000));
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_CPU_WINDOW_START_HIGH,
                (m64_start >> 32) & 0xFF);
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_CPU_WINDOW_END_HIGH,
                ((m64_end-1) >> 32) & 0xFF);

    // Mark the controller as a bridge
    WRITE_MEM32(pci_bcm2711_ctx->cfg_ptr + REG_PCI_ID_VAL,
                (0x6 << 16) | (0x4 << 8));

    // Note: PCI controller throws a Serror when trying to configure devices
    // Manually configuring the bus to prevent this

    pci_bridge_tree_t root_bridge = {
       .bus = 0,
       .dev = 0,
       .func = 0,
       .class = 0x06,
       .subclass = 0x04,
       .progif = 0x01,
       .primary_bus = 0,
       .secondary_bus = 1,
       .subordinate_bus = 1,
       .this_dev = create_pci_device(pci_ctx, 0, 0, 0),
       .children = NULL,
       .devices = llist_create()
    }; 


    pcie_apply_bridge_bus_config(pci_ctx, &root_bridge);

    pci_device_ctx_t* xhci_usb_dev = create_pci_device(pci_ctx, 1, 0, 0);
    llist_append_ptr(root_bridge.devices, xhci_usb_dev);

    pci_ctx->bridge_tree = root_bridge;

    pcie_fixup_bridge_tree(pci_ctx);

    print_pci_header(xhci_usb_dev);
    print_pci_capabilities(xhci_usb_dev);

    //uint32_t* xhci_ptr = xhci_usb_dev->bar[0].vmem;
    //console_log(LOG_INFO, "XHCI ptr: %16x", xhci_ptr);
    //uint32_t hciversion = *(volatile uint32_t*)(xhci_usb_dev->bar[0].vmem + 0);
    //console_log(LOG_INFO, "XHCI[2]: %4x", hciversion >> 16);

    discover_pcie(pci_ctx);
}

