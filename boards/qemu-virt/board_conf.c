
#include <stdint.h>
#include <stdbool.h>

#include "board_conf.h"

#include "drivers/virtio_pci_early_console/virtio_pci_early_console.h"

#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/dtb.h"
#include "kernel/exec.h"
#include "kernel/task.h"
#include "kernel/console.h"
#include "kernel/lib/libpci.h"
#include "kernel/fs_manager.h"
#include "kernel/kmalloc.h"

void board_init_main_early(void) {
}

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

extern uint8_t _bootstrap_text_start;
extern uint8_t _bootstrap_text_end;
extern uint8_t _bootstrap_data_start;
extern uint8_t _bootstrap_data_end;
extern uint8_t _text_start;
extern uint8_t _text_end;
extern uint8_t _data_start;
extern uint8_t _data_end;
extern uint8_t _bss_start;
extern uint8_t _bss_end;

void board_discover_devices(void) {

    mask_range_t mask_ranges[5] = {
        {.start = (uintptr_t)&_bootstrap_text_start, .end = (uintptr_t)&_bootstrap_text_end},
        {.start = (uintptr_t)&_text_start, .end = (uintptr_t)&_text_end},
        {.start = (uintptr_t)&_bootstrap_data_start, .end = (uintptr_t)&_bootstrap_data_end},
        {.start = (uintptr_t)&_data_start, .end = (uintptr_t)&_data_end},
        {.start = (uintptr_t)&_bss_start,  .end = (uintptr_t)&_bss_end},
    };
    for (int idx = 0; idx < 5; idx++) {
        mask_ranges[idx].start = KSPACE_TO_PHY(mask_ranges[idx].start);
        mask_ranges[idx].end = KSPACE_TO_PHY(mask_ranges[idx].end);
    }

    discover_phy_mem_dtb(mask_ranges, 5);
}

void board_loop() {

    int64_t open_res;
    open_res = fs_manager_mount_device("sys", "virtio_disk0", FS_TYPE_EXT2,
                                       "home");
    ASSERT(open_res >= 0);

    uint64_t echo_tid;
    char* echo_argv[] = {
        "Echo in userspace",
        NULL
    };
    echo_tid = exec_user_task("home", "bin/hello_rust.elf", "hello_rust", echo_argv);
    ASSERT(echo_tid != 0);

    while (true) {
        task_wait_timer_in(1000*1000);

        console_log(LOG_INFO, "Tick");
    }

}
