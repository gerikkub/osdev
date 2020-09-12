
#include <stdint.h>
#include <stdbool.h>

#include "bootstrap/vmem_bootstrap.h"

static _vmem_entry_t l0_table_bootstrap[VMEM_4K_TABLE_ENTRIES] __attribute__((section(".bootstrap.data"),aligned(4096)));
static _vmem_entry_t l1_table_bootstrap[VMEM_4K_TABLE_ENTRIES] __attribute__((section(".bootstrap.data"),aligned(4096)));

static _vmem_entry_t vmem_entry_block_bootstrap(uint16_t attr_hi, uint16_t attr_lo, uintptr_t addr, uint8_t block_level)
    __attribute__((section (".bootstrap.text")));
static _vmem_entry_t vmem_entry_table_bootstrap(uintptr_t addr, uint64_t flags)
    __attribute__((section (".bootstrap.text")));
void setup_vmem_bootstrap(void)
    __attribute__((section (".bootstrap.text")));

static _vmem_entry_t vmem_entry_block_bootstrap(uint16_t attr_hi, uint16_t attr_lo, uintptr_t addr, uint8_t block_level) {

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
    }
}

static _vmem_entry_t vmem_entry_table_bootstrap(uintptr_t addr, uint64_t flags) {

    return ((_vmem_entry_table_t)flags) |
           (((_vmem_entry_table_t)addr) & 0xFFFFFFFFF000) |
           3;
}

void setup_vmem_bootstrap(void) {
    // Create a bootstrap virtual memory mapping that maps
    // both the bootstrap code and kernel memory and data.
    // This is an identity mapping of the bottom 4 GB of each
    // address space. This mapping will be used for both high
    // and low address spaces. After creating this mapping the
    // bootstrap code will jump to the kernel in high memory
    // and create a better mapping.

    l0_table_bootstrap[0] = vmem_entry_table_bootstrap((uintptr_t)&l1_table_bootstrap, VMEM_STG1_TBL_NSTABLE);

    uint16_t block_attr_hi = 0;
    uint16_t block_attr_lo = VMEM_STG1_BLK_AF | // Prevent nusiance page faults during bootstrap
                             VMEM_STG1_BLK_NS | // Non-secure memory
                             VMEM_ATTR_NORMAL_NONCACHE |
                             0; // EL1 RW access. No kernel access


    // Map the bottom 4 GB of address space
    for (uint64_t idx = 0; idx < 4; idx++) {
        l1_table_bootstrap[idx] =
            vmem_entry_block_bootstrap(block_attr_hi,
                                       block_attr_lo,
                                       ((uintptr_t)idx) << 30,
                                       1);
    }

    // Setup translation base registers
    uint64_t ttbr_el1 = ((uintptr_t)l0_table_bootstrap) | 
                          0;

    WRITE_SYS_REG(TTBR0_EL1, ttbr_el1);
    WRITE_SYS_REG(TTBR1_EL1, ttbr_el1);

    // Setup translation registers

    uint64_t tcr_el1 = SYS_TCR_EL1_TBI1 | // Ignore top byte in address translations
                       SYS_TCR_EL1_TBI0 | // Ignore top byte in address translations
                       // ASID Size 8 bit
                       // IPS Phy address space 32 bits
                       SYS_TCR_EL1_TG11 | // 4K TTBR1_EL1 
                       // Non-shareable memory
                       // Non-cacheable
                       // Run Translation walks
                       // TTBR0_EL1.ASID defines the ASID
                       SYS_TCR_EL1_T1SZ(16) | // Minimum size for TTBR1 region
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

    // Enable translations 
    uint64_t sctlr_el1;

    READ_SYS_REG(SCTLR_EL1, sctlr_el1);

    sctlr_el1 |= 1; // M bit. Enable MMU address translation

    WRITE_SYS_REG(SCTLR_EL1, sctlr_el1);

    asm ("ISB");
}

