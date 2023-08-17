
#include <stdint.h>
#include <stdbool.h>

#include "board_conf.h"

#include "drivers/pl011_uart.h"

#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/dtb.h"
#include "kernel/lib/libpci.h"

void board_init_mappings(void) {

    memory_entry_device_t earlypl011_device = {
       .start = (uint64_t)PL011_VMEM,
       .end = (uint64_t)PL011_VMEM + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)PL011_PHY
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&earlypl011_device);

}

void board_init_early_console(void) {
    pl011_init(PL011_VMEM);
}

void board_init_devices(void) {

    dtb_init(DTB_VMEM);
}