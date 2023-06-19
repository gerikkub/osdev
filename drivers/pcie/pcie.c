
#include <stdint.h>
#include <string.h>

#include "kernel/drivers.h"

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libpci.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/llist.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

void pcie_discovered(void* ctx);

static discovery_register_t s_dtb_register = {
    .type = DRIVER_DISCOVERY_DTB,
    .dtb = {
        .compat = "pci-host-ecam-generic"
    },
    .ctxfunc = pcie_discovered
};

void pcie_register(void) {
    register_driver(&s_dtb_register);
}

REGISTER_DRIVER(pcie);

typedef struct {
    pci_addr_t io_pci_addr;
    uintptr_t io_mem_addr;
    uint64_t io_size;

    pci_addr_t m32_pci_addr;
    uintptr_t m32_mem_addr;
    uint64_t m32_size;

    pci_addr_t m64_pci_addr;
    uintptr_t m64_mem_addr;
    uint64_t m64_size;
} pci_range_t;

typedef struct {
    pci_range_t* range_ctx;
    uint64_t io_top;
    uint64_t m32_top;
    uint64_t m64_top;
} pci_alloc_t;

typedef struct {
    uint64_t device[3];
    uint64_t int_num;
    uint64_t int_ctx[3];
} pci_interrupt_map_entry_t;

typedef struct {
    uint32_t intid;
    llist_t* handler_list;
} pci_interrupt_ctx_t;

typedef struct {
    uint32_t device_mask[3];
    uint32_t int_mask;
    pci_interrupt_map_entry_t* entries;
    uint64_t num_entries;
    pci_interrupt_ctx_t int_ctx[4];
} pci_interrupt_map_t;

static pci_range_t s_pci_ranges;
static pci_alloc_t s_dtb_alloc;
static pci_interrupt_map_t s_pcie_int_map;

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

void create_pci_device(volatile pci_header0_t* header, uintptr_t header_phy, uint64_t offset) {

    discovery_register_t driver_discovery = {
        .type = DRIVER_DISCOVERY_PCI,
        .pci = {
            .vendor_id = header->vendor_id,
            .device_id = header->device_id
        },
    };

    bool have_driver = discovery_have_driver(&driver_discovery);
    if (!have_driver) {
        console_log(LOG_WARN, "No Driver for PCI %4x:%4x at offset %x",
                       header->vendor_id, header->device_id,
                       offset);
        return;
    }

    console_log(LOG_INFO, "Found driver for PCI %4x:%4x",
                          header->vendor_id, header->device_id);


    discovery_pci_ctx_t device_ctx = {0};
    device_ctx.header_phy = header_phy;
    device_ctx.header_offset = offset;
    //device_ctx.int_map = pcie_int_map;

    device_ctx.io_size = 0;
    device_ctx.m32_size = 0;
    device_ctx.m64_size = 0;

    bool io_aligned = false;
    bool m32_aligned = false;
    bool m64_aligned = false;

    int idx;
    for (idx = 0; idx < 6; idx++) {
        if ((header->bar[idx] & PCI_BAR_REGION_MASK) == PCI_BAR_REGION_IO) {
            // IO Region bar
            header->bar[idx] = 0;
            uint32_t bar_init = header->bar[idx];
            header->bar[idx] = 0xFFFFFFFF;
            uint32_t bar_set = header->bar[idx];

            uint32_t size = (~(bar_set ^ bar_init)) + 1;
            if (size == 0) {
                continue;
            }

            if (!io_aligned) {
                align_pci_memory(&s_dtb_alloc, 4096, PCI_SPACE_IO);
                io_aligned = true;
            }

            uint64_t pci_addr = alloc_pci_memory(&s_dtb_alloc, size, PCI_SPACE_IO);
            uintptr_t phy_addr = pci_to_mem_addr(&s_pci_ranges, pci_addr, PCI_SPACE_IO);

            device_ctx.bar[idx].allocated = true;
            device_ctx.bar[idx].len = size;
            device_ctx.bar[idx].space = PCI_SPACE_IO;
            device_ctx.bar[idx].phy = phy_addr;
            header->bar[idx] = pci_addr;

            if (!io_aligned) {
                device_ctx.io_base = pci_addr;
                io_aligned = true;
            }

            device_ctx.io_size = pci_addr + size - device_ctx.io_base;

        } else {
            // Memory Region bar
            if ((header->bar[idx] & PCI_BAR_MEM_LOC_MASK) == PCI_BAR_MEM_LOC_M32) {
                // 32-bit memory region

                header->bar[idx] = 0;
                uint32_t bar_init = header->bar[idx];
                header->bar[idx] = 0xFFFFFFFF;
                uint32_t bar_set = header->bar[idx];

                uint32_t size = (~(uint64_t)(bar_set ^ bar_init)) + 1;
                if (size == 0) {
                    continue;
                }

                if (!m32_aligned) {
                    align_pci_memory(&s_dtb_alloc, 4096, PCI_SPACE_M32);
                }

                uint64_t pci_addr = alloc_pci_memory(&s_dtb_alloc, size, PCI_SPACE_M32);
                uintptr_t phy_addr = pci_to_mem_addr(&s_pci_ranges, pci_addr, PCI_SPACE_M32);

                device_ctx.bar[idx].allocated = true;
                device_ctx.bar[idx].len = size;
                device_ctx.bar[idx].space = PCI_SPACE_M32;
                device_ctx.bar[idx].phy = phy_addr;
                header->bar[idx] = pci_addr;

                if (!m32_aligned) {
                    device_ctx.m32_base = pci_addr;
                    m32_aligned = true;
                }

                device_ctx.m32_size = pci_addr + size - device_ctx.m32_base;

            } else if ((header->bar[idx] & PCI_BAR_MEM_LOC_MASK) == PCI_BAR_MEM_LOC_M64) {
                // 64-bit memory region

                header->bar[idx] = 0;
                header->bar[idx+1] = 0;
                uint64_t bar_init = ((uint64_t)header->bar[idx+1] << 32) | header->bar[idx];
                header->bar[idx] = 0xFFFFFFFF;
                header->bar[idx+1] = 0xFFFFFFFF;
                uint64_t bar_set = ((uint64_t)header->bar[idx+1] << 32) | header->bar[idx];

                uint64_t size = (~(bar_set ^ bar_init)) + 1;
                if (size == 0) {
                    continue;
                }

                if (!m64_aligned) {
                    align_pci_memory(&s_dtb_alloc, 4096, PCI_SPACE_M64);
                }

                uint64_t pci_addr = alloc_pci_memory(&s_dtb_alloc, size, PCI_SPACE_M64);
                uintptr_t phy_addr = pci_to_mem_addr(&s_pci_ranges, pci_addr, PCI_SPACE_M64);
                device_ctx.bar[idx].allocated = true;
                device_ctx.bar[idx].len = size;
                device_ctx.bar[idx].space = PCI_SPACE_M64;
                device_ctx.bar[idx].phy = phy_addr;
                header->bar[idx] = pci_addr & 0xFFFFFFFFUL;
                header->bar[idx+1] = (pci_addr >> 32) & 0xFFFFFFFFUL;

                if (!m64_aligned) {
                    device_ctx.m64_base = pci_addr;
                    m64_aligned = true;
                }

                device_ctx.m64_size = pci_addr + size - device_ctx.m64_base;
                idx++;

            } else {
                device_ctx.bar[idx].allocated = false;
            }
        }
    }

    // Map BAR memory
    for (idx = 0; idx < 6; idx++) {
        if (device_ctx.bar[idx].allocated) {
            memspace_map_device_kernel((void*)device_ctx.bar[idx].phy, PHY_TO_KSPACE_PTR(device_ctx.bar[idx].phy),
                                       PAGE_CEIL(device_ctx.bar[idx].len), MEMSPACE_FLAG_PERM_KRW);
        }
    }
    memspace_update_kernel_vmem();

    header->command |= 7;
    
    discover_driver(&driver_discovery, (void*)&device_ctx);
}

void discover_pcie(void* pcie_mem, uint64_t len) {

    uint64_t offset;
    for (offset = 0; offset < len; offset += 512) {
        pci_header_t* header = (pci_header_t*)(pcie_mem + offset);

        if (header->vendor_id != 0xFFFF &&
            (header->header_type & 0x7F) == 0) {
            
            uint64_t header_phy = 0x4010000000 + offset;
            create_pci_device((pci_header0_t*)header, header_phy, offset);
            //print_pci_header(header, (void*)0x4010000000);
        }
    }
}


pci_range_t pcie_parse_ranges(dt_block_t* dt_block) {

    dt_node_t* dt_node = (dt_node_t*)&dt_block->data[dt_block->node_off];

    ASSERT(dt_node->std_properties_mask & DT_PROP_RANGES);
    dt_prop_range_entry_t* dt_ranges = (dt_prop_range_entry_t*)&dt_block->data[dt_node->ranges.range_entries_off];
    uint64_t dt_num_ranges = dt_node->ranges.num_ranges;

    pci_range_t ranges = {0};

    for (uint64_t idx = 0; idx < dt_num_ranges; idx++) {
        dt_prop_range_entry_t* this_dt_range = &dt_ranges[idx];
        uint64_t space = this_dt_range->pci_hi_addr & PCI_ADDR_HI_SPACE_MASK;

        pci_addr_t* pci_addr = NULL;

        switch (space) {
            case PCI_ADDR_HI_SPACE_CONFIG:
                break;
            case PCI_ADDR_HI_SPACE_IO:
                pci_addr = &ranges.io_pci_addr;
                ranges.io_mem_addr = this_dt_range->parent_addr;
                ranges.io_size = this_dt_range->size;
                pci_addr->space = PCI_SPACE_IO;
                break;
            case PCI_ADDR_HI_SPACE_M32:
                pci_addr = &ranges.m32_pci_addr;
                ranges.m32_mem_addr = this_dt_range->parent_addr;
                ranges.m32_size = this_dt_range->size;
                pci_addr->space = PCI_SPACE_M32;
                break;
            case PCI_ADDR_HI_SPACE_M64:
                pci_addr = &ranges.m64_pci_addr;
                ranges.m64_mem_addr = this_dt_range->parent_addr;
                ranges.m64_size = this_dt_range->size;
                pci_addr->space = PCI_SPACE_M64;
                break;
            default:
                ASSERT(false);
        }

        pci_addr->relocatable = this_dt_range->pci_hi_addr & PCI_ADDR_HI_RELOCATABLE;
        pci_addr->prefetchable = this_dt_range->pci_hi_addr & PCI_ADDR_HI_PREFETCHABLE;
        pci_addr->aliased = this_dt_range->pci_hi_addr & PCI_ADDR_HI_ALIASED;
        pci_addr->bus = (this_dt_range->pci_hi_addr & PCI_ADDR_HI_BUS_MASK) >> PCI_ADDR_HI_BUS_SHIFT;
        pci_addr->device = (this_dt_range->pci_hi_addr & PCI_ADDR_HI_DEVICE_MASK) >> PCI_ADDR_HI_DEVICE_SHIFT;
        pci_addr->function = (this_dt_range->pci_hi_addr & PCI_ADDR_HI_FUNCTION_MASK) >> PCI_ADDR_HI_FUNCTION_SHIFT;
        pci_addr->reg = (this_dt_range->pci_hi_addr & PCI_ADDR_HI_REGISTER_MASK) >> PCI_ADDR_HI_REGISTER_SHIFT;
        pci_addr->addr = this_dt_range->child_addr;
    }

    return ranges;
}

void* pcie_parse_allocate_reg(dt_block_t* dt_block) {

    dt_node_t* dt_node = (dt_node_t*)&dt_block->data[dt_block->node_off];

    ASSERT(dt_node->std_properties_mask & DT_PROP_REG);
    uint64_t dt_num_reg = dt_node->reg.num_regs;
    ASSERT(dt_num_reg == 1);
    dt_prop_reg_entry_t* dt_reg = (dt_prop_reg_entry_t*)&dt_block->data[dt_node->reg.reg_entries_off];

    memspace_map_device_kernel((void*)dt_reg->addr, PHY_TO_KSPACE_PTR(dt_reg->addr), dt_reg->size, MEMSPACE_FLAG_PERM_KRW);
    memspace_update_kernel_vmem();
    return PHY_TO_KSPACE_PTR(dt_reg->addr);
}

pci_interrupt_map_t* pcie_parse_interrupt_map(dt_block_t* dt_block) {

    dt_node_t* dt_node = (dt_node_t*)&dt_block->data[dt_block->node_off];

    pci_interrupt_map_t* map = vmalloc(sizeof(pci_interrupt_map_t));

    // Find interrupt-map-mask entry
    uint64_t prop_idx = 0;
    uint64_t* property_list = (uint64_t*)&dt_block->data[dt_node->property_list_off];
    dt_prop_generic_t* prop = NULL;
    while (prop_idx < dt_node->num_properties) {
       prop = (dt_prop_generic_t*)&dt_block->data[property_list[prop_idx]];

       char* name = (char*)&dt_block->data[prop->name_off];
       if (strncmp(name, "interrupt-map-mask", 19) == 0) {
           break;
       }

       prop_idx++;
    }

    ASSERT(prop_idx < dt_node->num_properties);
    ASSERT(prop->data_len == 16);

    uint32_t* prop_data = (uint32_t*)&dt_block->data[prop->data_off];

    map->device_mask[0] = __builtin_bswap32(prop_data[0]);
    map->device_mask[1] = __builtin_bswap32(prop_data[1]);
    map->device_mask[2] = __builtin_bswap32(prop_data[2]);
    map->int_mask = __builtin_bswap32(prop_data[3]);

    // Find interrupt-map
    while (prop_idx < dt_node->num_properties) {
       prop = (dt_prop_generic_t*)&dt_block->data[property_list[prop_idx]];

       char* name = (char*)&dt_block->data[prop->name_off];
       if (strncmp(name, "interrupt-map", 14) == 0) {
           break;
       }

       prop_idx++;
    }

    ASSERT(prop_idx < dt_node->num_properties);
    ASSERT(prop->data_len % 40 == 0);

    uint64_t map_count = prop->data_len / 40;

    map->entries = vmalloc(map_count * sizeof(pci_interrupt_map_entry_t));
    map->num_entries = map_count;

    prop_data = (uint32_t*)&dt_block->data[prop->data_off];

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

void pcie_discovered(void* ctx) {
    dt_block_t* dt_block = ((discovery_dtb_ctx_t*)ctx)->block;

    console_log(LOG_INFO, "Discovered Pcie\n");


    pci_range_t dtb_ranges = pcie_parse_ranges(dt_block);
    void* pcie_ptr = pcie_parse_allocate_reg(dt_block);

    pci_interrupt_map_t* pcie_int_map = pcie_parse_interrupt_map(dt_block);

    //char* name = (char*)&dt_block->data[dt_node->name_off];
    //dt_node_t* dt_node = (dt_node_t*)&dt_block->data[dt_block->node_off];

    /*
    console_printf("PCIE Interrupt Map:\n");
    for (uint64_t idx = 0; idx < pcie_int_map->num_entries; idx++) {
        pci_interrupt_map_entry_t* entry = &pcie_int_map->entries[idx];

        console_printf(" Device %x %x %x (%x %x %x)\n",
                       entry->device[0], entry->device[1], entry->device[2],
                       entry->device[0] & pcie_int_map->device_mask[0],
                       entry->device[1] & pcie_int_map->device_mask[1],
                       entry->device[2] & pcie_int_map->device_mask[2]);
        console_printf(" Int %2x (%2x)\n",
                       entry->int_num,
                       entry->int_num & pcie_int_map->int_mask);
        console_printf(" Int Ctx %2x %2x %2x\n",
                       entry->int_ctx[0],
                       entry->int_ctx[1],
                       entry->int_ctx[2]);
        console_printf("\n");
    }
    */

    s_pci_ranges = dtb_ranges;
    s_dtb_alloc.range_ctx = &s_pci_ranges;
    s_dtb_alloc.io_top = s_pci_ranges.io_pci_addr.addr;
    s_dtb_alloc.m32_top = s_pci_ranges.m32_pci_addr.addr;
    s_dtb_alloc.m64_top = s_pci_ranges.m64_pci_addr.addr;

    s_pcie_int_map = *pcie_int_map;
    vfree(pcie_int_map);

    pcie_init_interrupts();

    //discover_pcie(pcie_ptr, 0x10000000, pcie_int_map);
    discover_pcie(pcie_ptr, 0x10000000);
}