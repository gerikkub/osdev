
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
#include "kernel/gtimer.h"
#include "kernel/select.h"

#include "include/k_ioctl_common.h"
#include "include/k_select.h"
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

    int64_t gpio_fd;
    gpio_fd = vfs_open_device_fd("sys",
                                 "bcm2711_gpio",
                                 0);
    ASSERT(gpio_fd >= 0);

    fd_ctx_t* gpio_fd_ctx = get_kernel_fd(gpio_fd);
    ASSERT(gpio_fd_ctx != NULL);

    k_gpio_config_t gpio_config = {
        .gpio_num = 3,
        .flags = GPIO_CONFIG_FLAG_OUT_PP,
    };
    uint64_t config_args = (uint64_t)&gpio_config;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    gpio_config.gpio_num = 4;
    gpio_config.flags = GPIO_CONFIG_FLAG_OUT_PP;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    gpio_config.gpio_num = 17;
    gpio_config.flags = GPIO_CONFIG_FLAG_OUT_PP;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    gpio_config.gpio_num = 27;
    gpio_config.flags = GPIO_CONFIG_FLAG_OUT_PP;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    k_gpio_config_t spi_gpio_config = {
        .gpio_num = 8,
        .flags = GPIO_CONFIG_FLAG_AF_EN,
        .af = 0
    };
    config_args = (uint64_t)&spi_gpio_config;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 10;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 11;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 9;
    spi_gpio_config.flags |= GPIO_CONFIG_FLAG_PULL_DOWN;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    spi_gpio_config.gpio_num = 25;
    spi_gpio_config.flags = GPIO_CONFIG_FLAG_IN |
                            GPIO_CONFIG_FLAG_EV_FALLING;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);


    int64_t spi_fd;
    spi_fd = vfs_open_device_fd("sys", "spi0", 0);
    ASSERT(spi_fd >= 0);

    fd_ctx_t* spi_fd_ctx = get_kernel_fd(spi_fd);
    ASSERT(spi_fd_ctx != NULL);

    k_spi_device_t enc28j60_device = {
        .clk_hz = 8000000,
        .flags = 0
    };

    config_args = (uint64_t)&enc28j60_device;
    int64_t spi_dev_fd_num = spi_fd_ctx->ops.ioctl(spi_fd_ctx->ctx, SPI_IOCTL_CREATE_DEVICE, &config_args, 1);
    ASSERT(spi_dev_fd_num >= 0);

    enc28j60_discover_t enc28j60_disc = {
        .spi_fd = spi_dev_fd_num,
        .gpio_int_fd_ctx = gpio_fd_ctx,
        .gpio_int_num = 25
    };
    (void)enc28j60_disc;
    discover_driver_manual("enc28j60", &enc28j60_disc);
}

void kernel_gpio_irq_thread(void* ctx) {
    fd_ctx_t* gpio_fd_ctx = ctx;

    k_gpio_listener_t gpio_listener = {
        .gpio_num = 2
    };
    uint64_t ioctl_args = (uint64_t)&gpio_listener;
    int64_t listener_fd = gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_LISTENER, &ioctl_args, 1);
    ASSERT(listener_fd >= 0);

    k_gpio_level_t gpio_level = {
        .gpio_num = 4,
        .level = 0
    };
    ioctl_args = (uint64_t)&gpio_level;

    while (true) {
        syscall_select_ctx_t enc_irq_select = {
            .fd = listener_fd,
            .ready_mask = FD_READY_GPIO_EVENT
        };
        uint64_t gpio_mask_out;
        select_wait(&enc_irq_select, 1, UINT64_MAX, &gpio_mask_out);

        gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_SET, &ioctl_args, 1);
        gpio_level.level = gpio_level.level == 0;
    }
}

void board_loop() {

    fd_ops_t nic_ops;
    void* nic_ctx;
    int64_t open_res;
    open_res = vfs_open_device("sys",
                               //"virtio-pci-net0",
                               "enc28j60",
                               0,
                               &nic_ops,
                               &nic_ctx,
                               NULL);
    if (open_res < 0) {
        console_log(LOG_INFO, "virtio-pci-net0 not available. Skipping IP set");
    } else {
        uint64_t ip = 192 << 24 |
                      168 << 16 |
                      0 << 8 |
                    //   210;
                      211;
        int64_t res;
        res = nic_ops.ioctl(nic_ctx, NET_IOCTL_SET_IP, &ip, 1);
        ASSERT(res == 0);

        uint64_t ip_net = 192 << 24 |
                          168 << 16 |
                          0 << 8 |
                          0;

        uint64_t args[2] = {
            ip_net, 24
        };

        res = nic_ops.ioctl(nic_ctx, NET_IOCTL_SET_ROUTE, args, 2);

        uint64_t ip_route = 192 << 24 |
                            168 << 16 |
                            0 << 8 |
                            207;

        uint64_t args_default[2] = {
            ip_route, 24
        };
        res = nic_ops.ioctl(nic_ctx, NET_IOCTL_SET_DEFAULT_ROUTE, args_default, 2);
    }

    int64_t gpio_fd = vfs_open_device_fd("sys", "bcm2711_gpio", 0);
    ASSERT(gpio_fd >= 0);

    fd_ctx_t* gpio_fd_ctx = get_kernel_fd(gpio_fd);

    k_gpio_config_t gpio_config = {
        .gpio_num = 42,
        .flags = GPIO_CONFIG_FLAG_OUT_PP |
                 GPIO_CONFIG_FLAG_PULL_NONE |
                 GPIO_CONFIG_FLAG_EV_NONE
    };

    uint64_t config_args = (uint64_t)&gpio_config;

    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);

    gpio_config.gpio_num = 2;
    gpio_config.flags = GPIO_CONFIG_FLAG_OUT_PP |
                        GPIO_CONFIG_FLAG_EV_FALLING;
    gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_CONFIGURE, &config_args, 1);


    k_gpio_level_t gpio_level = {
        .gpio_num = 42,
        .level = 1
    };
    uint64_t level_args = (uint64_t)&gpio_level;


    create_kernel_task(0x10000, kernel_gpio_irq_thread, gpio_fd_ctx, "gpio-test");

    uint64_t ticknum = 0;
    while (1) {
        task_wait_timer_in(1000*1000);

        ticknum++;

        gpio_level.gpio_num = 42;
        gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_SET, &level_args, 1);

        gpio_level.gpio_num = 2;
        gpio_fd_ctx->ops.ioctl(gpio_fd_ctx->ctx, GPIO_IOCTL_SET, &level_args, 1);

        gpio_level.level = !gpio_level.level;
    }


}