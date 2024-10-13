
#include <stdint.h>
#include <string.h>

#include "kernel/drivers.h"

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libpci.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"

#include "drivers/pcie/pcie.h"

/*
void pcie_irq_handler(uint32_t intid, void* ctx) {
    pci_interrupt_ctx_t* int_ctx = ctx;
    llist_t* pci_handlers = int_ctx->handler_list;
    pcie_int_handler_ctx_t* handler;

    FOR_LLIST(pci_handlers, handler) {
        ASSERT(handler != NULL);
        ASSERT(handler.fn != NULL);
        handler.fn(handler.ctx);
    }
    END_FOR_LLIST()
}
*/

void align_pci_memory(pci_alloc_t* allocator, uint64_t alignment, pci_space_t space) {

    uint64_t space_base = 0;
    uint64_t top = 0;
    uint64_t max_size = 0;
    switch (space) {
        case PCI_SPACE_IO:
            top = allocator->io_top;
            max_size = allocator->range_ctx->io_size;
            space_base = allocator->range_ctx->io_pci_addr.addr;
            break;
        case PCI_SPACE_M32:
            top = allocator->m32_top;
            max_size = allocator->range_ctx->m32_size;
            space_base = allocator->range_ctx->m32_pci_addr.addr;
            break;
        case PCI_SPACE_M64:
            top = allocator->m64_top;
            max_size = allocator->range_ctx->m64_size;
            space_base = allocator->range_ctx->m64_pci_addr.addr;
            break;
        default:
            ASSERT(0);
    }

    uint64_t aligned_base = (top + alignment - 1) & (~(alignment - 1));
    top = aligned_base;
    if (top > space_base + max_size) {
        // TODO: Don't fail silently
        return;
    }

    switch (space) {
        case PCI_SPACE_IO:
            allocator->io_top = top;
            break;
        case PCI_SPACE_M32:
            allocator->m32_top = top;
            break;
        case PCI_SPACE_M64:
            allocator->m64_top = top;
            break;
        default:
            ASSERT(0);
    }
}

uint64_t alloc_pci_memory(pci_alloc_t* allocator, uint64_t size, pci_space_t space) {

    if (size < VMEM_PAGE_SIZE) {
        size = VMEM_PAGE_SIZE;
    }

    uint64_t top = 0;
    uint64_t max_size = 0;
    uint64_t space_base = 0;
    switch (space) {
        case PCI_SPACE_IO:
            top = allocator->io_top;
            max_size = allocator->range_ctx->io_size;
            space_base = allocator->range_ctx->io_pci_addr.addr;
            break;
        case PCI_SPACE_M32:
            top = allocator->m32_top;
            max_size = allocator->range_ctx->m32_size;
            space_base = allocator->range_ctx->m32_pci_addr.addr;
            break;
        case PCI_SPACE_M64:
            top = allocator->m64_top;
            max_size = allocator->range_ctx->m64_size;
            space_base = allocator->range_ctx->m64_pci_addr.addr;
            break;
        default:
            ASSERT(0);
    }

    uint64_t aligned_base = (top + size - 1) & (~(size - 1));
    top = aligned_base + size;
    if (top > space_base + max_size) {
        return 0;
    }

    switch (space) {
        case PCI_SPACE_IO:
            allocator->io_top = top;
            break;
        case PCI_SPACE_M32:
            allocator->m32_top = top;
            break;
        case PCI_SPACE_M64:
            allocator->m64_top = top;
            break;
        default:
            ASSERT(0);
    }

    return aligned_base;
}

uintptr_t pci_to_mem_addr(pci_range_t* ranges, uint64_t pci_addr, pci_space_t space) {
    ASSERT(ranges != NULL);

    uint64_t mem_base = 0;
    uint64_t pci_base = 0;
    uint64_t size = 0;
    switch (space) {
        case PCI_SPACE_IO:
            mem_base = ranges->io_mem_addr;
            pci_base = ranges->io_pci_addr.addr;
            size = ranges->io_size;
            break;
        case PCI_SPACE_M32:
            mem_base = ranges->m32_mem_addr;
            pci_base = ranges->m32_pci_addr.addr;
            size = ranges->m32_size;
            break;
        case PCI_SPACE_M64:
            mem_base = ranges->m64_mem_addr;
            pci_base = ranges->m64_pci_addr.addr;
            size = ranges->m64_size;
            break;
        default:
            ASSERT(0);
    }

    uint64_t offset = pci_addr - pci_base;
    if (offset < size &&
        pci_addr >= pci_base) {
        return offset + mem_base;
    } else {
        // Memory address would be outside of valid memory
        return 0;
    }
}

uint64_t mem_to_pci_addr(pci_range_t* ranges, uintptr_t mem_addr, pci_space_t space) {

    ASSERT(ranges != NULL);

    uint64_t mem_base = 0;
    uint64_t pci_base = 0;
    uint64_t size = 0;
    switch (space) {
        case PCI_SPACE_IO:
            mem_base = ranges->io_mem_addr;
            pci_base = ranges->io_pci_addr.addr;
            size = ranges->io_size;
            break;
        case PCI_SPACE_M32:
            mem_base = ranges->m32_mem_addr;
            pci_base = ranges->m32_pci_addr.addr;
            size = ranges->m32_size;
            break;
        case PCI_SPACE_M64:
            mem_base = ranges->m64_mem_addr;
            pci_base = ranges->m64_pci_addr.addr;
            size = ranges->m64_size;
            break;
        default:
            ASSERT(0);
    }

    uint64_t offset = mem_addr - mem_base;
    if (offset < size &&
        mem_addr >= mem_base) {
        return offset + pci_base;
    } else {
        // Memory address would be outside of valid memory
        return 0;
    }
}

pci_device_ctx_t* create_pci_device(pci_ctx_t* pci_ctx, uint8_t bus, uint8_t dev, uint8_t func) {
    
    pci_device_ctx_t* pci_dev = vmalloc(sizeof(pci_device_ctx_t));

    pci_dev->pci_ctx = pci_ctx;
    pci_dev->bus = bus;
    pci_dev->dev = dev;
    pci_dev->func = func;

    pci_dev->vendor_id = PCI_HREAD16(pci_ctx, PCI_DEV_ADDR(pci_dev, 0), offsetof(pci_header_t, vendor_id));
    pci_dev->device_id = PCI_HREAD16(pci_ctx, PCI_DEV_ADDR(pci_dev, 0), offsetof(pci_header_t, device_id));

    pci_dev->io_size = 0;
    pci_dev->m32_size = 0;
    pci_dev->m64_size = 0;

    console_log(LOG_INFO, "PCIe %2x:%2x:%2x Create (%16x)",
                pci_dev->bus, pci_dev->dev, pci_dev->func, pci_dev);
    uint64_t addr = PCI_GEN_ADDR(bus, dev, func, 0);

    pci_dev->cap_list = llist_create();
    uint32_t cap_off = PCI_HREAD8_DEV(pci_dev, offsetof(pci_header0_t, capabilities_ptr)) & 0xFC;
    while (cap_off != 0) {
        pci_cap_t* cap = vmalloc(sizeof(pci_cap_t));
        cap->offset = cap_off;
        cap->cap = PCI_HREAD8_DEV(pci_dev, cap_off + PCI_CAP_CAP);
        cap->ctx = NULL;
        console_log(LOG_INFO, " Found Cap %2x (%2x)", cap->cap, cap_off);
        llist_append_ptr(pci_dev->cap_list, cap);

        cap_off = PCI_HREAD8_DEV(pci_dev, cap_off + PCI_CAP_NEXT);
    }

    int idx;
    for (idx = 0; idx < 6; idx++) {
        console_log(LOG_INFO, " Reading BAR %d", idx);
        const uint32_t bar_val = PCI_READBAR(pci_ctx, addr, idx);
        console_log(LOG_INFO, " BAR val %8x", bar_val);
        if ((bar_val & PCI_BAR_REGION_MASK) == PCI_BAR_REGION_IO &&
            pci_ctx->ranges.io_size > 0) {
            // IO Region bar
            PCI_WRITEBAR(pci_ctx, addr, idx, 0);
            uint32_t bar_init = PCI_READBAR(pci_ctx, addr, idx);
            PCI_WRITEBAR(pci_ctx, addr, idx, 0xFFFFFFFF);
            uint32_t bar_set = PCI_READBAR(pci_ctx, addr, idx);

            uint32_t size = (~(bar_set ^ bar_init)) + 1;
            if (size == 0) {
                continue;
            }

            uint64_t pci_addr = alloc_pci_memory(&pci_ctx->dtb_alloc, size, PCI_SPACE_IO);
            uintptr_t phy_addr = pci_to_mem_addr(&pci_ctx->ranges, pci_addr, PCI_SPACE_IO);

            pci_dev->bar[idx].allocated = true;
            pci_dev->bar[idx].len = size;
            pci_dev->bar[idx].space = PCI_SPACE_IO;
            pci_dev->bar[idx].phy = phy_addr;
            pci_dev->bar[idx].vmem = PHY_TO_KSPACE_PTR(phy_addr);
            PCI_WRITEBAR(pci_ctx, addr, idx, pci_addr);
            console_log(LOG_INFO, " [%d] Allocated IO [%8x:%8x:%8x]",
                        idx, pci_addr, phy_addr, size);

            if (pci_dev->io_size == 0) {
                pci_dev->io_base_phy = pci_addr;
                pci_dev->io_base_vmem = PHY_TO_KSPACE_PTR(pci_addr);
            }

            pci_dev->io_size = pci_addr + size - pci_dev->io_base_phy;

        } else {
            // Memory Region bar
            if ((bar_val & PCI_BAR_MEM_LOC_MASK) == PCI_BAR_MEM_LOC_M32 &&
                pci_ctx->ranges.m32_size > 0) {
                // 32-bit memory region

                PCI_WRITEBAR(pci_ctx, addr, idx, 0);
                uint32_t bar_init = PCI_READBAR(pci_ctx, addr, idx);
                PCI_WRITEBAR(pci_ctx, addr, idx, 0xFFFFFFFF);
                uint32_t bar_set = PCI_READBAR(pci_ctx, addr, idx);

                uint32_t size = (~(uint64_t)(bar_set ^ bar_init)) + 1;
                if (size == 0) {
                    continue;
                }

                uint64_t pci_addr = alloc_pci_memory(&pci_ctx->dtb_alloc, size, PCI_SPACE_M32);
                uintptr_t phy_addr = pci_to_mem_addr(&pci_ctx->ranges, pci_addr, PCI_SPACE_M32);

                pci_dev->bar[idx].allocated = true;
                pci_dev->bar[idx].len = size;
                pci_dev->bar[idx].space = PCI_SPACE_M32;
                pci_dev->bar[idx].phy = phy_addr;
                pci_dev->bar[idx].vmem = PHY_TO_KSPACE_PTR(phy_addr);
                PCI_WRITEBAR(pci_ctx, addr, idx, pci_addr);
                console_log(LOG_INFO, " [%d] Allocated M32 [%8x:%8x:%8x]",
                            idx, pci_addr, phy_addr, size);

                if (pci_dev->m32_size == 0) {
                    pci_dev->m32_base_phy = pci_addr;
                    pci_dev->m32_base_vmem = PHY_TO_KSPACE_PTR(pci_addr);
                }

                pci_dev->m32_size = pci_addr + size - pci_dev->m32_base_phy;

            } else if ((bar_val & PCI_BAR_MEM_LOC_MASK) == PCI_BAR_MEM_LOC_M64) {
                // 64-bit memory region
                if (pci_ctx->ranges.m64_size == 0) {
                    idx++;
                    continue;;
                }

                PCI_WRITEBAR(pci_ctx, addr, idx, 0);
                PCI_WRITEBAR(pci_ctx, addr, idx+1, 0);
                uint64_t bar_init = ((uint64_t)PCI_READBAR(pci_ctx, addr, idx+1) << 32) |
                                      PCI_READBAR(pci_ctx, addr, idx);
                PCI_WRITEBAR(pci_ctx, addr, idx, 0xFFFFFFFF);
                PCI_WRITEBAR(pci_ctx, addr, idx+1, 0xFFFFFFFF);
                uint64_t bar_set = ((uint64_t)PCI_READBAR(pci_ctx, addr, idx+1) << 32) |
                                      PCI_READBAR(pci_ctx, addr, idx);

                uint64_t size = (~(bar_set ^ bar_init)) + 1;
                if (size == 0) {
                    continue;
                }

                uint64_t pci_addr = alloc_pci_memory(&pci_ctx->dtb_alloc, size, PCI_SPACE_M64);
                uintptr_t phy_addr = pci_to_mem_addr(&pci_ctx->ranges, pci_addr, PCI_SPACE_M64);
                pci_dev->bar[idx].allocated = true;
                pci_dev->bar[idx].len = size;
                pci_dev->bar[idx].space = PCI_SPACE_M64;
                pci_dev->bar[idx].phy = phy_addr;
                pci_dev->bar[idx].vmem = PHY_TO_KSPACE_PTR(phy_addr);
                PCI_WRITEBAR(pci_ctx, addr, idx, pci_addr & 0xFFFFFFFFUL);
                PCI_WRITEBAR(pci_ctx, addr, idx+1, (pci_addr >> 32) & 0xFFFFFFFFUL);
                console_log(LOG_INFO, " [%d] Allocated M64 [%16x:%16x:%16x]",
                            idx, pci_addr, phy_addr, size);

                if (pci_dev->m64_size == 0) {
                    pci_dev->m64_base_phy = pci_addr;
                    pci_dev->m64_base_vmem = PHY_TO_KSPACE_PTR(pci_addr);
                }

                pci_dev->m64_size = pci_addr + size - pci_dev->m64_base_phy;
                idx++;

            } else {
                pci_dev->bar[idx].allocated = false;
            }
        }
    }

    if (pci_dev->io_size) {
        console_log(LOG_INFO, " IO Range [%8x:%8x]",
                    pci_dev->io_base_phy, pci_dev->io_base_phy + pci_dev->io_size);
    }
    if (pci_dev->m32_size) {
        console_log(LOG_INFO, " M32 Range [%8x:%8x]",
                    pci_dev->m32_base_phy, pci_dev->m32_base_phy + pci_dev->m32_size);
    }
    if (pci_dev->m64_size) {
        console_log(LOG_INFO, " M64 Range [%8x:%8x]",
                    pci_dev->m64_base_phy, pci_dev->m64_base_phy + pci_dev->m64_size);
    }


    // Map BAR memory
    for (idx = 0; idx < 6; idx++) {
        if (pci_dev->bar[idx].allocated) {
            memspace_map_device_kernel((void*)pci_dev->bar[idx].phy, PHY_TO_KSPACE_PTR(pci_dev->bar[idx].phy),
                                       PAGE_CEIL(pci_dev->bar[idx].len), MEMSPACE_FLAG_PERM_KRW);
        }
    }
    memspace_update_kernel_vmem();

    print_pci_capabilities(pci_dev);

    uint16_t cmd = PCI_HREAD16(pci_ctx, addr, offsetof(pci_header0_t, command));
    PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header0_t, command), cmd | 7);

    pci_dev->msix_vector_list = llist_create();
    pci_dev->msix_cap = pci_get_capability(pci_dev, PCI_CAP_MSIX, 0);

    if (pci_dev->msix_cap) {


        uint32_t num_msix_entries = (PCI_READCAP16_DEV(pci_dev,
                                                       pci_dev->msix_cap,
                                                       PCI_CAP_MSIX_MSG_CTRL)
                                      & PCI_MSIX_CTRL_SIZE_MASK) + 1;

        bitalloc_init(&pci_dev->msix_vector_alloc, 0, num_msix_entries, vmalloc);
    }

    return pci_dev;
}

void pcie_apply_bridge_bus_config(pci_ctx_t* pci_ctx, pci_bridge_tree_t* node) {

    if (node->subclass == 0) {
        // Don't do anything for host bridges
        return;
    }

    uint64_t addr = PCI_GEN_ADDR(node->bus, node->dev, node->func, 0);
    PCI_HWRITE8(pci_ctx, addr, offsetof(pci_header1_t, primary_bus), node->primary_bus);
    PCI_HWRITE8(pci_ctx, addr, offsetof(pci_header1_t, secondary_bus), node->secondary_bus);
    PCI_HWRITE8(pci_ctx, addr, offsetof(pci_header1_t, subordinate_bus), node->subordinate_bus);

    uint16_t cmd = PCI_HREAD16(pci_ctx, addr, offsetof(pci_header1_t, command));
    cmd |= 1 << 2;
    PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, command), cmd);
    MEM_DMB();
}

static uint8_t build_recursive_topology(pci_ctx_t* pci_ctx, pci_bridge_tree_t* node) {

    uint8_t max_bus = node->secondary_bus;
    for (int dev = 0; dev < 0x1F; dev++) {
        //console_log(LOG_DEBUG, "Checking dev %2x:%2x", node->secondary_bus, dev);
        uint64_t addr = PCI_GEN_ADDR(node->secondary_bus, dev, 0, 0);
        uint16_t vendor_id = PCI_HREAD16(pci_ctx, addr, offsetof(pci_header_t, vendor_id));
        //console_log(LOG_DEBUG, "Vendor is %4x", vendor_id);

        if (vendor_id == 0xFFFF) {
            continue;
        }

        console_log(LOG_INFO, "Found device (%4x) at %2x:%2x",
                    vendor_id, node->secondary_bus, dev);

        uint8_t class = PCI_HREAD8(pci_ctx, addr, offsetof(pci_header_t, class));
        uint8_t subclass = PCI_HREAD8(pci_ctx, addr, offsetof(pci_header_t, subclass));
        uint8_t progif = PCI_HREAD8(pci_ctx, addr, offsetof(pci_header_t, prog_if));

        if ((class == 0x6) && 
           (((subclass == 0x4) || (subclass == 0x9)))) {

            console_log(LOG_INFO, "Found PCI-PCI bridge at %2x:%2x", node->secondary_bus, dev);

            bool ok;
            uint8_t child_bus = linalloc_alloc(&pci_ctx->bus_alloc, 1, &ok);
            ASSERT(ok);
            pci_bridge_tree_t* child_bridge = vmalloc(sizeof(pci_bridge_tree_t));
            child_bridge->bus = node->secondary_bus;
            child_bridge->dev = dev;
            child_bridge->func = 0;
            child_bridge->class = class;
            child_bridge->subclass = subclass;
            child_bridge->progif = progif;
            child_bridge->primary_bus = node->secondary_bus;
            child_bridge->secondary_bus = child_bus;
            child_bridge->subordinate_bus = 0xFF;
            child_bridge->children = llist_create();
            child_bridge->devices = llist_create();

            llist_append_ptr(node->children, child_bridge);

            pcie_apply_bridge_bus_config(pci_ctx, child_bridge);
            max_bus = build_recursive_topology(pci_ctx, child_bridge);
        } else {
            pci_device_ctx_t* pci_dev = create_pci_device(pci_ctx,
                                                          node->secondary_bus,
                                                          dev, 0);
            llist_append_ptr(node->devices, pci_dev);
        }
    }

    node->subordinate_bus = max_bus;
    pcie_apply_bridge_bus_config(pci_ctx, node);

    return max_bus;

}

void scan_pcie_topology(pci_ctx_t* pci_ctx) {

    uint8_t root_type, root_class, root_subclass, root_progif;

    root_type = PCI_HREAD8(pci_ctx,
                           PCI_GEN_ADDR(0, 0, 0, 0),
                           offsetof(pci_header_t, header_type));
    root_class = PCI_HREAD8(pci_ctx,
                            PCI_GEN_ADDR(0, 0, 0, 0),
                            offsetof(pci_header_t, class));
    root_subclass = PCI_HREAD8(pci_ctx,
                               PCI_GEN_ADDR(0, 0, 0, 0),
                               offsetof(pci_header_t, subclass));
    root_progif = PCI_HREAD8(pci_ctx,
                             PCI_GEN_ADDR(0, 0, 0, 0),
                             offsetof(pci_header_t, prog_if));

    if (root_class != 0x06) {
        uint16_t vid = PCI_HREAD16(pci_ctx,
                                   PCI_GEN_ADDR(0, 0, 0, 0),
                                   offsetof(pci_header_t, vendor_id));
        uint16_t did = PCI_HREAD16(pci_ctx,
                                   PCI_GEN_ADDR(0, 0, 0, 0),
                                   offsetof(pci_header_t, device_id));

        console_log(LOG_WARN, "Root PCI device %4x:%4x type not a bridge %2x %2x %2x",
                    vid, did,
                    root_type, root_class, root_subclass);
        return;
    }

    console_log(LOG_INFO, "Found root PCI device: %2x %2x %2x %2x",
                root_type, root_class, root_subclass, root_progif);

    pci_bridge_tree_t root_bridge = {
        .bus = 0,
        .dev = 0,
        .func = 0,
        .class = root_class,
        .subclass = root_subclass,
        .progif = root_progif,
        .primary_bus = 0,
        .secondary_bus = 1,
        .subordinate_bus = 0xFF,
        .children = llist_create(),
        .devices = llist_create(),
    };

    if (root_subclass == 0) {
        root_bridge.secondary_bus = 0;
    }

    pci_ctx->bridge_tree = root_bridge;
    linalloc_init(&pci_ctx->bus_alloc, 2, 0xFF, 1);
    pcie_apply_bridge_bus_config(pci_ctx, &root_bridge);

    build_recursive_topology(pci_ctx, &root_bridge);
}

static void pcie_fixup_bridge_tree_internal(pci_ctx_t* pci_ctx, pci_bridge_tree_t* bridge) {

    uint64_t io_base = UINT64_MAX;
    uint64_t io_limit = 0;
    uint64_t m32_base = UINT64_MAX;
    uint64_t m32_limit = 0;
    uint64_t m64_base = UINT64_MAX;
    uint64_t m64_limit = 0;
    pci_bridge_tree_t* child_bridge;
    if (bridge->children != NULL) {
        FOR_LLIST(bridge->children, child_bridge)
            pcie_fixup_bridge_tree_internal(pci_ctx, child_bridge);

            io_base = MIN(io_base, child_bridge->io_base);
            io_limit = MAX(io_limit, child_bridge->io_limit);
            m32_base = MIN(m32_base, child_bridge->m32_base);
            m32_limit = MAX(m32_limit, child_bridge->m32_limit);
            m64_base = MIN(m64_base, child_bridge->m64_base);
            m64_limit = MAX(m64_limit, child_bridge->m64_limit);
        END_FOR_LLIST()
    }

    pci_device_ctx_t* pci_dev;
    if (bridge->devices != NULL) {
        FOR_LLIST(bridge->devices, pci_dev)
            io_base = MIN(io_base, pci_dev->io_base_phy);
            io_limit = MAX(io_limit, pci_dev->io_base_phy + pci_dev->io_size);
            m32_base = MIN(m32_base, pci_dev->m32_base_phy);
            m32_limit = MAX(m32_limit, pci_dev->m32_base_phy + pci_dev->m32_size);
            m64_base = MIN(m64_base, pci_dev->m64_base_phy);
            m64_limit = MAX(m64_limit, pci_dev->m64_base_phy + pci_dev->m64_size);
        END_FOR_LLIST()
    }

    bridge->io_base = io_base;
    bridge->io_limit = io_limit;
    bridge->m32_base = m32_base;
    bridge->m32_limit = m32_limit;
    bridge->m64_base = m64_base;
    bridge->m64_limit = m64_limit;

    if (bridge->subclass == 0) {
        return;
    }

    console_log(LOG_DEBUG, "Bridge %2x:%2x:%2x IO: [%6x:%6x] M32: [%4x:%4x] M64: [%12x:%12x]",
                bridge->bus, bridge->dev, bridge->func,
                io_base, io_limit,
                m32_base, m32_limit,
                m64_base, m64_limit);

    uint64_t addr = PCI_GEN_ADDR(bridge->bus, bridge->dev, bridge->func, 0);
    if (io_limit != 0) {
        ASSERT(io_base < (1 << 24));
        ASSERT(io_limit < (1 << 24));
        PCI_HWRITE8(pci_ctx, addr, offsetof(pci_header1_t, io_base), io_base & 0xFF);
        PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, io_base_upper), (io_base >> 16) & 0xFFFF);
        PCI_HWRITE8(pci_ctx, addr, offsetof(pci_header1_t, io_limit), io_limit & 0xFF);
        PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, io_limit_upper), (io_limit >> 16) & 0xFFFF);
    }

    if (m32_limit != 0) {
        ASSERT(m32_base < (1 << 16));
        ASSERT(m32_limit < (1 << 16));
        PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, memory_base), m32_base & 0xFFFF);
        PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, memory_limit), m32_limit & 0xFFFF);
    }

    if (m64_limit != 0) {
        ASSERT(m64_base < (1UL << 48));
        ASSERT(m64_limit < (1UL << 48));
        PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, prefetch_base), m64_base & 0xFFFF);
        PCI_HWRITE32(pci_ctx, addr, offsetof(pci_header1_t, prefetch_base_upper), (m64_base >> 16) & 0xFFFFFFFF);
        PCI_HWRITE16(pci_ctx, addr, offsetof(pci_header1_t, prefetch_limit), m64_limit & 0xFFFF);
        PCI_HWRITE32(pci_ctx, addr, offsetof(pci_header1_t, prefetch_limit_upper), (m64_limit >> 16) & 0xFFFFFFFF);
    }
}

void pcie_fixup_bridge_tree(pci_ctx_t* pci_ctx) {
    pcie_fixup_bridge_tree_internal(pci_ctx, &pci_ctx->bridge_tree);
}

void discover_pcie_internal(pci_ctx_t* pci_ctx, pci_bridge_tree_t* bridge_node) {

    if (bridge_node->children != NULL) {
        pci_bridge_tree_t* child_bridge;
        FOR_LLIST(bridge_node->children, child_bridge)
            discover_pcie_internal(pci_ctx, child_bridge);
        END_FOR_LLIST()
    }

    if (bridge_node->devices != NULL) {
        pci_device_ctx_t* child_dev;
        FOR_LLIST(bridge_node->devices, child_dev)
            discovery_register_t disc = {
                .type = DRIVER_DISCOVERY_PCI,
                .pci = {
                    .vendor_id = child_dev->vendor_id,
                    .device_id = child_dev->device_id,
                }
            };

            console_log(LOG_INFO, "PCIe Discovering %4x:%4x",
                        child_dev->vendor_id,
                        child_dev->device_id);

            print_pci_header(child_dev);
            print_pci_capabilities(child_dev);

            discover_driver(&disc, child_dev);
        END_FOR_LLIST()
    }
}

void discover_pcie(pci_ctx_t* pci_ctx) {
    discover_pcie_internal(pci_ctx, &pci_ctx->bridge_tree);
}

pci_range_t pcie_parse_ranges(dt_node_t* dt_node) {

    ASSERT(dt_node->prop_ranges != NULL);

    dt_prop_range_entry_t* dt_ranges = dt_node->prop_ranges->range_entries;
    uint64_t dt_num_ranges = dt_node->prop_ranges->num_ranges;

    pci_range_t ranges = {0};

    for (uint64_t idx = 0; idx < dt_num_ranges; idx++) {
        dt_prop_range_entry_t* this_dt_range = &dt_ranges[idx];

        ASSERT(this_dt_range->addr_size == 3);
        ASSERT(this_dt_range->paddr_size == 2);
        ASSERT(this_dt_range->size_size == 2);


        uint32_t pci_hi_addr = this_dt_range->addr_ptr[0];

        uint64_t addr = this_dt_range->addr_ptr[2] | (((uint64_t)this_dt_range->addr_ptr[1]) << 32);
        uint64_t paddr = this_dt_range->paddr_ptr[1] | (((uint64_t)this_dt_range->paddr_ptr[0]) << 32);
        uint64_t size = this_dt_range->size_ptr[1] | (((uint64_t)this_dt_range->size_ptr[0]) << 32);

        bool ok;
        paddr = dt_map_addr_to_phy(dt_node, paddr, &ok);
        ASSERT(ok);

        uint64_t space = pci_hi_addr & PCI_ADDR_HI_SPACE_MASK;

        console_log(LOG_INFO, "PCIE Range: %8x %16x %16x %16x", pci_hi_addr, addr, paddr, size);

        pci_addr_t* pci_addr = NULL;

        switch (space) {
            case PCI_ADDR_HI_SPACE_CONFIG:
                break;
            case PCI_ADDR_HI_SPACE_IO:
                pci_addr = &ranges.io_pci_addr;
                ranges.io_mem_addr = paddr;
                ranges.io_size = size;
                pci_addr->space = PCI_SPACE_IO;
                break;
            case PCI_ADDR_HI_SPACE_M32:
                pci_addr = &ranges.m32_pci_addr;
                ranges.m32_mem_addr = paddr;
                ranges.m32_size = size;
                pci_addr->space = PCI_SPACE_M32;
                break;
            case PCI_ADDR_HI_SPACE_M64:
                pci_addr = &ranges.m64_pci_addr;
                ranges.m64_mem_addr = paddr;
                ranges.m64_size = size;
                pci_addr->space = PCI_SPACE_M64;
                break;
            default:
                ASSERT(false);
        }

        pci_addr->relocatable = pci_hi_addr & PCI_ADDR_HI_RELOCATABLE;
        pci_addr->prefetchable = pci_hi_addr & PCI_ADDR_HI_PREFETCHABLE;
        pci_addr->aliased = pci_hi_addr & PCI_ADDR_HI_ALIASED;
        pci_addr->bus = (pci_hi_addr & PCI_ADDR_HI_BUS_MASK) >> PCI_ADDR_HI_BUS_SHIFT;
        pci_addr->device = (pci_hi_addr & PCI_ADDR_HI_DEVICE_MASK) >> PCI_ADDR_HI_DEVICE_SHIFT;
        pci_addr->function = (pci_hi_addr & PCI_ADDR_HI_FUNCTION_MASK) >> PCI_ADDR_HI_FUNCTION_SHIFT;
        pci_addr->reg = (pci_hi_addr & PCI_ADDR_HI_REGISTER_MASK) >> PCI_ADDR_HI_REGISTER_SHIFT;
        pci_addr->addr = addr;
    }

    return ranges;
}

void* pcie_parse_allocate_reg(dt_node_t* dt_node, int64_t* size_out) {

    ASSERT(dt_node->prop_reg != NULL);
    uint64_t dt_num_reg = dt_node->prop_reg->num_regs;
    ASSERT(dt_num_reg == 1);
    dt_prop_reg_entry_t* dt_reg = dt_node->prop_reg->reg_entries;

    ASSERT(dt_reg->addr_size == 2);
    ASSERT(dt_reg->size_size == 2);

    uint64_t addr = dt_reg->addr_ptr[1] | (((uint64_t)dt_reg->addr_ptr[0]) << 32);
    uint64_t size = dt_reg->size_ptr[1] | (((uint64_t)dt_reg->size_ptr[0]) << 32);

    bool ok;
    addr = dt_map_addr_to_phy(dt_node, addr, &ok);
    ASSERT(ok);

    console_log(LOG_INFO, "PCIE Reg: %16x %8x", addr, size);

    memspace_map_device_kernel((void*)addr, PHY_TO_KSPACE_PTR(addr), size, MEMSPACE_FLAG_PERM_KRW);
    memspace_update_kernel_vmem();
    *size_out = size;
    return PHY_TO_KSPACE_PTR(addr);
}

pci_interrupt_map_t* pcie_parse_interrupt_map(dt_node_t* dt_node) {

    pci_interrupt_map_t* map = vmalloc(sizeof(pci_interrupt_map_t));

    // Find interrupt-map-mask entry
    dt_prop_generic_t* prop = NULL;
    dt_prop_generic_t* prop_tmp = NULL;
    FOR_LLIST(dt_node->properties, prop_tmp)
       if (strncmp(prop_tmp->name, "interrupt-map-mask", 19) == 0) {
            prop = prop_tmp;
       }

    END_FOR_LLIST()

    ASSERT(prop != NULL);
    ASSERT(prop->data_len == 16);

    uint32_t* prop_data = (uint32_t*)prop->data;

    map->device_mask[0] = __builtin_bswap32(prop_data[0]);
    map->device_mask[1] = __builtin_bswap32(prop_data[1]);
    map->device_mask[2] = __builtin_bswap32(prop_data[2]);
    map->int_mask = __builtin_bswap32(prop_data[3]);

    // Find interrupt-map
    prop = NULL;
    FOR_LLIST(dt_node->properties, prop_tmp)
       if (strncmp(prop_tmp->name, "interrupt-map", 14) == 0) {
           prop = prop_tmp;
       }
    END_FOR_LLIST()

    ASSERT(prop != NULL);
    ASSERT(prop->data_len % 40 == 0);

    uint64_t map_count = prop->data_len / 40;

    map->entries = vmalloc(map_count * sizeof(pci_interrupt_map_entry_t));
    map->num_entries = map_count;

    prop_data = (uint32_t*)prop->data;

    for (uint64_t idx = 0; idx < map_count; idx++) {
        map->entries[idx].device[0] = __builtin_bswap32(prop_data[0]);
        map->entries[idx].device[1] = __builtin_bswap32(prop_data[1]);
        map->entries[idx].device[2] = __builtin_bswap32(prop_data[2]);
        map->entries[idx].int_num = __builtin_bswap32(prop_data[3]);
        map->entries[idx].int_ctx[0] = __builtin_bswap32(prop_data[7]);
        map->entries[idx].int_ctx[1] = __builtin_bswap32(prop_data[8]);
        map->entries[idx].int_ctx[2] = __builtin_bswap32(prop_data[9]);

        prop_data += 10;
    }

    return map;
}

void pcie_alloc_irq(uint32_t intid) {

/*
    for (uint64_t idx = 0; idx < 4; idx++) {
        if (s_pcie_int_map.int_ctx[idx].handler_lists == NULL) {
            break;
        }
    }

    ASSERT(idx < 4);
    s_pcie_int_map.int_ctx[idx].handler_list = llist_create();
    s_pcie_int_map.int_ctx[idx].intid = intid;

    interrupt_register_irq_handler(intid, pcie_irq_handler, &s_pcie_int_map.int_ctx[idx]);
    */
}

void pcie_init_interrupts() {

/*
    pci_interrupt_map_t* pcie_int_map = &s_pcie_int_map;

    int64_t irq_nums[4];
    irq_nums[0] = -1;
    irq_nums[1] = -1;
    irq_nums[2] = -1;
    irq_nums[3] = -1;
    uint64_t max_irq_num = 0;

    for (uint64_t idx = 0; idx < pcie_int_map->num_entries; idx++) {
        uint64_t entry_irq_num = pcie_int_map->entries[idx].int_ctx[1];

        uint64_t int_idx  = 0;
        for (; int_idx < max_irq_num; int_idx++) {
            if (irq_nums[int_idx] == entry_irq_num) {
                break;
            }
        }

        if (int_idx != max_irq_num) {
            continue;
        } else {
            ASSERT(max_irq_num < 4);
            // New irq number
            pcie_alloc_irq(entry_irq_num);
            irq_nums[int_idx] = entry_irq_num;
            max_irq_num++;
        }
    }
    */
}

