
#include <stdint.h>
#include <stdbool.h>

#include "board_conf.h"

#include "drivers/pl011_uart.h"
#include "drivers/net/enc28j60/enc28j60.h"

#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/dtb.h"
#include "kernel/lib/libpci.h"
#include "kernel/fd.h"
#include "kernel/console.h"
#include "kernel/drivers.h"
#include "kernel/vfs.h"
#include "kernel/task.h"

#include "include/k_ioctl_common.h"
#include "include/k_gpio.h"
#include "include/k_spi.h"


int64_t raspi4b_uart_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    int64_t size_copy = size;
    volatile uint8_t* uart_tx_ptr = ctx;
    while (size_copy--) {
        *uart_tx_ptr = *buffer++;
        for (volatile int i = 500; i > 0; i--);
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
    uint8_t* uart_tx_ptr = (uint8_t*)0x47E215040;
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
    uint8_t* uart_tx_ptr = (uint8_t*)0xFFFF00047E215040;
    console_add_driver(&early_uart_ops, uart_tx_ptr);
}

void board_init_devices(void) {

    uint64_t* init_reg_ptr = PHY_TO_KSPACE_PTR(0x84000);

    dtb_init(init_reg_ptr[0]);
}

void board_discover_devices(void) {

    fd_ops_t gpio_ops;
    void* gpio_ctx;
    int64_t open_res;
    open_res = vfs_open_device("sys",
                               "bcm2711_gpio",
                               0,
                               &gpio_ops,
                               &gpio_ctx,
                               NULL);
    ASSERT(open_res >= 0);

    k_gpio_config_t spi_gpio_config = {
        .gpio_num = 8,
        .flags = GPIO_CONFIG_FLAG_AF_EN,
        .af = 0
    };
    uint64_t config_args = (uint64_t)&spi_gpio_config;
    gpio_ops.ioctl(gpio_ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 10;
    gpio_ops.ioctl(gpio_ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 11;
    gpio_ops.ioctl(gpio_ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 9;
    spi_gpio_config.flags |= GPIO_CONFIG_FLAG_PULL_DOWN;
    gpio_ops.ioctl(gpio_ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    fd_ops_t spi_ops;
    void* spi_ctx;
    open_res = vfs_open_device("sys",
                               "spi0",
                               0,
                               &spi_ops,
                               &spi_ctx,
                               NULL);
    ASSERT(open_res >= 0);

    k_spi_device_t enc28j60_device = {
        .clk_hz = 1000000,
        .flags = 0
    };

    config_args = (uint64_t)&enc28j60_device;
    int64_t spi_dev_fd_num = spi_ops.ioctl(spi_ctx, SPI_IOCTL_CREATE_DEVICE, &config_args, 1);
    ASSERT(spi_dev_fd_num >= 0);

    enc28j60_discover_t enc28j60_disc = {
        .spi_fd = spi_dev_fd_num
    };
    discover_driver_manual("enc28j60", &enc28j60_disc);
}