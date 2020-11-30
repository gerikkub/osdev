
#include <stdint.h>
#include <stdbool.h>

#include "kernel/vmem.h"
#include "kernel/kmalloc.h"
#include "stdlib/bitutils.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/kernelspace.h"


#define VMEM_ENTRY_IS_INVALID(x) (((x) & 1) == 0)
#define VMEM_ENTRY_IS_BLOCK(x) (((x) & 3) == 1)
#define VMEM_ENTRY_IS_TABLE(x) (((x) & 3) == 3)
#define VMEM_ENTRY_IS_PAGE(x) (((x) & 3) == 3)

#define VMEM_GET_VIRT_ADDR(l0, l1, l2, l3) \
    (((l0) << 39) | \
     ((l1) << 30) | \
     ((l2) << 21) | \
     ((l3) << 12))

extern uint8_t _text_start;
extern uint8_t _text_end;
extern uint8_t _data_start;
extern uint8_t _data_end;
extern uint8_t _bss_start;
extern uint8_t _bss_end;

_vmem_entry_t vmem_entry_invalid(void) {
    return 0;
}

_vmem_entry_t vmem_entry_block(uint16_t attr_hi, uint16_t attr_lo, uintptr_t addr, uint8_t block_level) {

    if (block_level == 1) {
        return ((_vmem_entry_block_t)attr_hi) |
               ((_vmem_entry_block_t)attr_lo) |
               (((_vmem_entry_block_t)addr) & 0xFFFFC0000000) |
               1;
    } else if (block_level == 2) {
        return ((_vmem_entry_block_t)attr_hi) |
               ((_vmem_entry_block_t)attr_lo) |
               (((_vmem_entry_block_t)addr) & 0xFFFFFFE00000) |
               1;
    } else {
        ASSERT(0);
        return 0;
    }
}

_vmem_entry_t vmem_entry_table(uintptr_t addr, uint64_t flags) {

    return ((_vmem_entry_table_t)flags) |
           (((_vmem_entry_table_t)addr) & 0xFFFFFFFFF000) |
           3;
}

_vmem_entry_t vmem_entry_page(uint16_t attr_hi, uint16_t attr_lo, uintptr_t addr) {
    return ((_vmem_entry_block_t)attr_hi) |
           ((_vmem_entry_block_t)attr_lo) |
           (((_vmem_entry_block_t)addr) & 0xFFFFFFFFF000) |
           3;
}


uint64_t vmem_get_block_addr(_vmem_entry_block_t block, uint8_t block_level) {

    ASSERT(VMEM_ENTRY_IS_BLOCK(block));
    ASSERT(block_level <= 2);
    ASSERT(block_level >= 1);

    if (block_level == 1) {
        return block & 0xFFFFC0000000;
    } else if (block_level == 2) {
        return block & 0xFFFFFFE00000;
    } else {
        ASSERT(0);
        return 0;
    }
}

uint64_t vmem_get_block_attr_hi(_vmem_entry_block_t block) {
    return (block >> 52) & 0xFFF;
}

uint64_t vmem_get_block_attr_lo(_vmem_entry_block_t block) {
    return (block >> 2) & 0x3FF;
}

_vmem_table* vmem_get_table_addr(_vmem_entry_table_t table) {
    return (_vmem_table*)(table & 0xFFFFFFFFF000);
}

addr_phy_t vmem_get_page_addr(_vmem_entry_page_t page) {
    return (addr_phy_t)(page & 0xFFFFFFFFF000);
}

_vmem_table* vmem_allocate_empty_table(void) {
    _vmem_table* table_ptr = (_vmem_table*)kmalloc_phy(VMEM_4K_TABLE_ENTRIES * sizeof(_vmem_entry_t));

    ASSERT(table_ptr != NULL);

    for (int idx = 0; idx < VMEM_4K_TABLE_ENTRIES; idx++) {
        table_ptr[idx] = vmem_entry_invalid();
    }

    return table_ptr;
}

void vmem_map_address(_vmem_table* table_ptr, addr_phy_t addr_phy, addr_virt_t addr_virt, _vmem_ap_flags ap_flags, vmem_attr_t mem_attr) {

    ASSERT(table_ptr != NULL);

    uintptr_t level_0_idx = (addr_virt >> 39) & 0x1FF;
    uintptr_t level_1_idx = (addr_virt >> 30) & 0x1FF;
    uintptr_t level_2_idx = (addr_virt >> 21) & 0x1FF;
    uintptr_t level_3_idx = (addr_virt >> 12) & 0x1FF;

    // Level 0 Table Walk
    ASSERT(level_0_idx < 512);
    _vmem_entry_t level_0_entry = table_ptr[level_0_idx];

    _vmem_table* level_1_table_ptr = NULL;

    // Not handling this case right now
    ASSERT(!VMEM_ENTRY_IS_BLOCK(level_0_entry));

    if (VMEM_ENTRY_IS_INVALID(level_0_entry)) {
        level_1_table_ptr = vmem_allocate_empty_table();

        uint64_t table_flags = VMEM_STG1_TBL_NSTABLE; // Nonsecure memory
                               // Let later entries decide AP, UXN, PXN

        table_ptr[level_0_idx] = vmem_entry_table((uintptr_t)level_1_table_ptr,
                                                  table_flags);

    } else {
        ASSERT(VMEM_ENTRY_IS_TABLE(level_0_entry));

        level_1_table_ptr = (_vmem_table*)PHY_TO_KSPACE(vmem_get_table_addr(level_0_entry));
    }

    ASSERT(level_1_table_ptr != NULL);

    // Level 1 Table Walk
    ASSERT(level_1_idx < 512);
    _vmem_entry_t level_1_entry = level_1_table_ptr[level_1_idx];

    _vmem_table* level_2_table_ptr = NULL;

    // Not handling this case right now
    ASSERT(!VMEM_ENTRY_IS_BLOCK(level_1_entry));

    if (VMEM_ENTRY_IS_INVALID(level_1_entry)) {
        level_2_table_ptr = vmem_allocate_empty_table();

        uint64_t table_flags = VMEM_STG1_TBL_NSTABLE; // Nonsecure memory
                               // Let later entries decide AP, UXN, PXN

        level_1_table_ptr[level_1_idx] = vmem_entry_table((uintptr_t)level_2_table_ptr,
                                                  table_flags);

    } else {
        ASSERT(VMEM_ENTRY_IS_TABLE(level_1_entry));

        level_2_table_ptr = (_vmem_table*)PHY_TO_KSPACE(vmem_get_table_addr(level_1_entry));
    }

    ASSERT(level_2_table_ptr != NULL);

    // Level 2 Table Walk
    ASSERT(level_2_idx < 512);
    _vmem_entry_t level_2_entry = level_2_table_ptr[level_2_idx];

    _vmem_table* level_3_table_ptr = NULL;

    // Not handling this case right now
    ASSERT(!VMEM_ENTRY_IS_BLOCK(level_2_entry));

    if (VMEM_ENTRY_IS_INVALID(level_2_entry)) {
        level_3_table_ptr = vmem_allocate_empty_table();

        uint64_t table_flags = VMEM_STG1_TBL_NSTABLE; // Nonsecure memory
                               // Let later entries decide AP, UXN, PXN

        level_2_table_ptr[level_2_idx] = vmem_entry_table((uintptr_t)level_3_table_ptr,
                                                  table_flags);

    } else {
        ASSERT(VMEM_ENTRY_IS_TABLE(level_2_entry));

        level_3_table_ptr = (_vmem_table*)PHY_TO_KSPACE(vmem_get_table_addr(level_2_entry));
    }

    ASSERT(level_3_table_ptr != NULL);


    // Level 2 Table Walk
    ASSERT(level_3_idx < 512);
    _vmem_entry_t level_3_entry = level_3_table_ptr[level_3_idx];

    ASSERT(VMEM_ENTRY_IS_INVALID(level_3_entry));

    uint64_t attr_hi = VMEM_AP_ALLOW_UX(ap_flags) ? 0 : VMEM_STG1_BLK_UXN |
                       VMEM_AP_ALLOW_PX(ap_flags) ? 0 : VMEM_STG1_BLK_PXN;
                       // Not Contiguous

    uint64_t attr_lo = // Global
                       // Access Flag (AF)?
                       // Non Shareable
                       VMEM_STG1_BLK_AF | // Set AF to 1
                       VMEM_STG1_BLK_NS |
                       VMEM_ATTR_NORMAL_NONCACHE |
                       VMEM_AP_EXTRACT(ap_flags) |
                       mem_attr; // Device memory mapped to attr 1

    level_3_table_ptr[level_3_idx] = vmem_entry_page(attr_hi, attr_lo, addr_phy);
}

void vmem_map_address_range(_vmem_table* table_ptr, addr_phy_t addr_phy, addr_virt_t addr_virt,
                            uint64_t len, _vmem_ap_flags ap_flags, vmem_attr_t mem_attr) {

    // Lengths must be multiples of 4K
    ASSERT((len & (VMEM_PAGE_SIZE - 1)) == 0);

    uint64_t pages = PAGE_CEIL(len) / VMEM_PAGE_SIZE;

    ASSERT(pages > 0);
    ASSERT(pages < 0x80000000);

    int idx;
    for (idx = 0; idx < pages; idx++) {
        vmem_map_address(table_ptr,
                         addr_phy + (idx * VMEM_PAGE_SIZE),
                         addr_virt + (idx * VMEM_PAGE_SIZE),
                         ap_flags, mem_attr);
    }

}

void vmem_map_range_flat(_vmem_table* table_ptr, addr_phy_t addr_lo, addr_phy_t addr_hi, _vmem_ap_flags ap_flags) {

    ASSERT(table_ptr != NULL);

    ASSERT((addr_lo & 0xFFF) == 0)
    ASSERT((addr_hi & 0xFFF) == 0)
    ASSERT(addr_lo <= addr_hi);

    addr_phy_t addr_map = addr_lo;

    if (addr_lo == addr_hi) {
        vmem_map_address(table_ptr, addr_lo, addr_lo, ap_flags, VMEM_ATTR_MEM);
    } else {

        // For each page, map the physical address to an indentical virtual address
        for (addr_map = addr_lo; addr_map < addr_hi; addr_map += 0x1000) {
            vmem_map_address(table_ptr, addr_map, addr_map, ap_flags, VMEM_ATTR_MEM);
        }
    }
}

_vmem_table* vmem_create_kernel_map(void) {

/*
    _vmem_table* level0_table_ptr = vmem_allocate_empty_table();

    ASSERT(level0_table_ptr != NULL);

    _vmem_ap_flags ap_flags = VMEM_AP_P_R |
                              VMEM_AP_P_W |
                              VMEM_AP_P_E;

    // Map text, data, bss. Page alignment guarenteed by the linker script
    vmem_map_range_flat(level0_table_ptr, ((addr_phy_t)&_text_start) & 0xFFFFFFFF, ((addr_phy_t)&_text_end) & 0xFFFFFFFF, ap_flags);

    if (((addr_phy_t)&_data_start) != ((addr_phy_t)&_data_end)) {
        vmem_map_range_flat(level0_table_ptr, ((addr_phy_t)&_data_start) & 0xFFFFFFFF, ((addr_phy_t)&_data_end) & 0xFFFFFFFF, ap_flags);
    }
    vmem_map_range_flat(level0_table_ptr, ((addr_phy_t)&_bss_start) & 0xFFFFFFFF, ((addr_phy_t)&_bss_end) & 0xFFFFFFFF, ap_flags);

    return level0_table_ptr;*/
    return NULL;
}

void vmem_set_tables(_vmem_table* kernel_ptr, _vmem_table* user_ptr) {

    ASSERT(kernel_ptr != NULL);
    ASSERT(user_ptr != NULL);

    uint64_t ttbr0_el1 = ((uintptr_t)user_ptr) | 
                          0;

    uint64_t ttbr1_el1 = ((uintptr_t)kernel_ptr) | 
                          0;

    asm ("DSB SY");

    WRITE_SYS_REG(TTBR0_EL1, ttbr0_el1);
    WRITE_SYS_REG(TTBR1_EL1, ttbr1_el1);
}

void vmem_set_user_table(_vmem_table* user_ptr, uint8_t asid) {

    ASSERT(user_ptr != NULL);

    uint64_t ttbr0_el1 = (((uintptr_t)user_ptr) & 0xFFFFFFFFFFFF) |
                          ((uint64_t)asid << 56);

    asm ("DSB SY");

    WRITE_SYS_REG(TTBR0_EL1, ttbr0_el1);
}

void vmem_initialize(void) { 

    uint64_t tcr_el1 = SYS_TCR_EL1_TBI1 | // Ignore top byte in address translations
                       SYS_TCR_EL1_TBI0 | // Ignore top byte in address translations
                       // ASID Size 8 bit
                       // IPS Phy address space 32 bits
                       SYS_TCR_EL1_TG11 | // 4K TTBR1_EL1 
                       // Non-shareable memory
                       // Non-cacheable
                       // Run Translation walks
                       // TTBR0_EL1.ASID defines the ASID
                       SYS_TCR_EL1_T1SZ(39) | // Minimum size for TTBR1 region
                       // 4K TTBR0_EL1
                       // Non-shareable memory
                       // Non-cacheable
                       // Run Translation walks
                       SYS_TCR_EL1_T0SZ(16); // 48 bit size for TTBR0 region


    WRITE_SYS_REG(TCR_EL1, tcr_el1);


    uint64_t mair_el1 = ((0x3 << 4) | 0x4) | // Attr 0. Normal memory Non-cacheable
                        (0 << 8); // Attr 1. Device Memory

    WRITE_SYS_REG(MAIR_EL1, mair_el1);

    asm ("ISB");
}

void vmem_enable_translations(void) {

    uint64_t sctlr_el1;

    READ_SYS_REG(SCTLR_EL1, sctlr_el1);

    sctlr_el1 |= 1; // M bit. Enable MMU address translation

    WRITE_SYS_REG(SCTLR_EL1, sctlr_el1);

    asm ("ISB");
}

void vmem_free_table(_vmem_table* table_ptr, uint64_t level) {

    ASSERT(table_ptr != NULL);
    ASSERT(level < 3);
    uint64_t idx;

    if (level < 2) {
        for (idx = 0; idx < VMEM_4K_TABLE_ENTRIES; idx++) {
            _vmem_entry_t entry = table_ptr[idx];
            if (VMEM_ENTRY_IS_TABLE(entry)) {
                _vmem_table* next_table = vmem_get_table_addr(entry);
                vmem_free_table(next_table, level+1);
                kfree_phy((uint8_t*)next_table);
            }
        }
    } else {
        for (idx = 0; idx < VMEM_4K_TABLE_ENTRIES; idx++) {
            _vmem_entry_t entry = table_ptr[idx];
            if (VMEM_ENTRY_IS_TABLE(entry)) {
                _vmem_table* next_table = vmem_get_table_addr(entry);
                kfree_phy((uint8_t*)next_table);
            }
        }
    }
}

bool vmem_walk_table(_vmem_table* table_ptr, uint64_t vmem_addr, uint64_t* phy_addr) {
    
    ASSERT(table_ptr != NULL);
    
    uint64_t l0_idx = (vmem_addr >> 39) & 0x1FF;
    uint64_t l1_idx = (vmem_addr >> 30) & 0x1FF;
    uint64_t l2_idx = (vmem_addr >> 21) & 0x1FF;
    uint64_t l3_idx = (vmem_addr >> 12) & 0x1FF;
    uint64_t page_offset = vmem_addr & 0xFFF;

    _vmem_entry_t l0_entry = table_ptr[l0_idx];
    if (!VMEM_ENTRY_IS_TABLE(l0_entry)) {
        return false;
    }
    _vmem_table* l1_table = (_vmem_table*)PHY_TO_KSPACE(vmem_get_table_addr(l0_entry));

    _vmem_entry_t l1_entry = l1_table[l1_idx];
    if (!VMEM_ENTRY_IS_TABLE(l1_entry)) {
        return false;
    }
    _vmem_table* l2_table = (_vmem_table*)PHY_TO_KSPACE(vmem_get_table_addr(l1_entry));

    _vmem_entry_t l2_entry = l2_table[l2_idx];
    if (!VMEM_ENTRY_IS_TABLE(l2_entry)) {
        return false;
    }
    _vmem_table* l3_table = (_vmem_table*)PHY_TO_KSPACE(vmem_get_table_addr(l2_entry));

    _vmem_entry_t l3_entry = l3_table[l3_idx];
    if (!VMEM_ENTRY_IS_PAGE(l3_entry)) {
        return false;
    }

    uintptr_t page_addr = PHY_TO_KSPACE(vmem_get_page_addr(l3_entry));
    if (phy_addr != NULL) {
        *phy_addr = page_addr + page_offset;
    }

    return true;
}

void vmem_print_l0_table(_vmem_table* table_ptr) {

    ASSERT(table_ptr != NULL);

    console_write("Page Table: \r\n");
    for (uint64_t l0_idx = 0; l0_idx < VMEM_4K_TABLE_ENTRIES; l0_idx++) {
        _vmem_entry_t l0_entry = table_ptr[l0_idx];

        if (VMEM_ENTRY_IS_TABLE(l0_entry)) {
            _vmem_table* l1_table = vmem_get_table_addr(l0_entry);
            ASSERT(l1_table != NULL);
            for (uint64_t l1_idx = 0; l1_idx < VMEM_4K_TABLE_ENTRIES; l1_idx++) {
                _vmem_entry_t l1_entry = l1_table[l1_idx];

                if (VMEM_ENTRY_IS_TABLE(l1_entry)) {
                    _vmem_table* l2_table = vmem_get_table_addr(l1_entry);
                    ASSERT(l2_table != NULL);
                    for (uint64_t l2_idx = 0; l2_idx < VMEM_4K_TABLE_ENTRIES; l2_idx++) {
                        _vmem_entry_t l2_entry = l2_table[l2_idx];

                        if (VMEM_ENTRY_IS_TABLE(l2_entry)) {
                            _vmem_table* l3_table = vmem_get_table_addr(l2_entry);
                            ASSERT(l3_table != NULL);

                            for (uint64_t l3_idx = 0; l3_idx < VMEM_4K_TABLE_ENTRIES; l3_idx++) {
                                _vmem_entry_t page = l3_table[l3_idx];

                                if (VMEM_ENTRY_IS_PAGE(page)) {

                                    console_write_hex_fixed(VMEM_GET_VIRT_ADDR(l0_idx, l1_idx, l2_idx, l3_idx), 12);
                                    console_write(" >> ");
                                    console_write_hex_fixed(vmem_get_page_addr(page), 12);
                                    console_endl();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}

