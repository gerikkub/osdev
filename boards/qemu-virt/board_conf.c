
#include <stdint.h>
#include <stdbool.h>

#include "board_conf.h"

#include "drivers/virtio_pci_early_console/virtio_pci_early_console.h"

#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/dtb.h"
#include "kernel/lib/libpci.h"

void board_init_mappings(void) {

    memory_entry_device_t earlycon_device = {
       .start = (uint64_t)EARLY_CON_VIRT,
       .end = (uint64_t)EARLY_CON_VIRT + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)EARLY_CON_PHY_BASE
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&earlycon_device);

    memory_entry_device_t earlypci_device = {
       .start = (uint64_t)EARLY_PCI_VIRT,
       .end = (uint64_t)EARLY_PCI_VIRT + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)EARLY_PCI_PHY_BASE
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&earlypci_device);
}

void board_init_early_console(void) {
    pci_poke_bar_entry((pci_header0_t*)EARLY_PCI_VIRT, 4, 16396);
    pci_poke_bar_entry((pci_header0_t*)EARLY_PCI_VIRT, 5, 128);
    virtio_pci_early_console_init((uint32_t*)(EARLY_CON_VIRT + EARLY_CON_BASE_OFFSET));
}

void board_init_devices(void) {

    discovery_register_t pl011_reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "arm,pl011\0arm,primecell"
        },
        .ctxfunc = NULL
    };

    driver_disable(&pl011_reg);

    dtb_init(DTB_VMEM);
}