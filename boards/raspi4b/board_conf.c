
#include <stdint.h>
#include <stdbool.h>

#include "board_conf.h"

#include "drivers/pl011_uart.h"

#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/dtb.h"
#include "kernel/lib/libpci.h"
#include "kernel/fd.h"
#include "kernel/console.h"


int64_t raspi4b_uart_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    int64_t size_copy = size;
    volatile uint8_t* uart_tx_ptr = ctx;
    while (size_copy--) {
        *uart_tx_ptr = *buffer++;
        for (volatile int i = 10000; i > 0; i--);
    }
    return size;
}

fd_ops_t early_uart_ops = {
    .read = NULL,
    .write = raspi4b_uart_write_op,
    .ioctl = NULL,
    .close = NULL
};

void board_init_main_early(void) {
    uint8_t* uart_tx_ptr = (uint8_t*)0xFE215040;
    console_add_driver(&early_uart_ops, uart_tx_ptr);

}

void board_init_mappings(void) {

    memory_entry_device_t earlyuart_device = {
       .start = (uint64_t)AUX_BASE,
       .end = (uint64_t)AUX_BASE + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)AUX_BASE_PHY
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&earlyuart_device);

}

void board_init_early_console(void) {
    uint8_t* uart_tx_ptr = (uint8_t*)0xFFFF0000FE215040;
    console_add_driver(&early_uart_ops, uart_tx_ptr);
}

void board_init_devices(void) {

    uint64_t* init_reg_ptr = PHY_TO_KSPACE_PTR(0x84000);

    /*
    for (int i = 0; i < 32; i+=2) {
        console_log(LOG_INFO, " x%d: %16x x%d: %16x", i, init_reg_ptr[i], i+1, init_reg_ptr[i+1]);
    }

    uint32_t* mem_dump = PHY_TO_KSPACE_PTR(init_reg_ptr[0]);
    for (int i = 0; i < 32; i++) {
        console_log(LOG_INFO, " %16x: %8x %8x %8x %8x",
                    (uint64_t)mem_dump + i*16,
                    mem_dump[i*4],
                    mem_dump[i*4+1],
                    mem_dump[i*4+2],
                    mem_dump[i*4+3]);
    }
    */

    dtb_init(init_reg_ptr[0]);
}