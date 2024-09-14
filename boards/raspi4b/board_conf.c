
#include <stdint.h>
#include <stdbool.h>

#include "board_conf.h"

#include "drivers/net/enc28j60/enc28j60.h"
#include "drivers/loop/loop.h"

#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/exec.h"
#include "kernel/drivers.h"
#include "kernel/dtb.h"
#include "kernel/fd.h"
#include "kernel/console.h"
#include "kernel/drivers.h"
#include "kernel/vfs.h"
#include "kernel/task.h"
#include "kernel/select.h"
#include "kernel/sys_device.h"
#include "kernel/fs_manager.h"
#include "kernel/net/udp_socket.h"
#include "kernel/lib/libtftp.h"

#include "include/k_ioctl_common.h"
#include "include/k_syscall.h"
#include "include/k_select.h"
#include "include/k_gpio.h"
#include "include/k_spi.h"
#include "include/k_net_api.h"

static uint8_t* s_uart_tx_ptr;

int64_t raspi4b_uart_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    int64_t size_copy = size;
    volatile uint8_t* uart_tx_ptr = s_uart_tx_ptr;
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
    s_uart_tx_ptr = uart_tx_ptr;
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
    s_uart_tx_ptr = uart_tx_ptr;
    console_add_driver(&early_uart_ops, uart_tx_ptr);
}

void board_init_devices(void) {

    uint64_t* init_reg_ptr = PHY_TO_KSPACE_PTR(0x84000);

    dtb_init(init_reg_ptr[0]);
}

int64_t raspi4b_uart_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    *ctx_out = ctx;
    return 0;
}

void board_discover_devices(void) {

    uint8_t* uart_tx_ptr = (uint8_t*)0x47E215040;
    sys_device_register(&early_uart_ops, raspi4b_uart_open_op, uart_tx_ptr, "con0");

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

    spi_gpio_config.gpio_num = 7;
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
        .gpio_int_num =7 
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

    int64_t nic_fd;
    nic_fd = vfs_open_device_fd("sys", "enc28j60", 0);
    fd_ctx_t* nic_fd_ctx = NULL;

    if (nic_fd < 0) {
        console_log(LOG_INFO, "enc28j60 not available. Skipping IP set");
    } else {
        nic_fd_ctx = get_kernel_fd(nic_fd);
        uint64_t ip = 192 << 24 |
                      168 << 16 |
                      0 << 8 |
                    //   210;
                      211;
        int64_t res;
        res = fd_call_ioctl(nic_fd_ctx, NET_IOCTL_SET_IP, &ip, 1);
        ASSERT(res == 0);

        uint64_t ip_net = 192 << 24 |
                          168 << 16 |
                          0 << 8 |
                          0;

        uint64_t args[2] = {
            ip_net, 24
        };

        res = fd_call_ioctl(nic_fd_ctx, NET_IOCTL_SET_ROUTE, args, 2);

        uint64_t ip_route = 192 << 24 |
                            168 << 16 |
                            0 << 8 |
                            207;

        uint64_t args_default[2] = {
            ip_route, 24
        };
        res = fd_call_ioctl(nic_fd_ctx, NET_IOCTL_SET_DEFAULT_ROUTE, args_default, 2);
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

    fd_call_ioctl_ptr(gpio_fd_ctx, GPIO_IOCTL_CONFIGURE, &gpio_config);

    gpio_config.gpio_num = 2;
    gpio_config.flags = GPIO_CONFIG_FLAG_OUT_PP |
                        GPIO_CONFIG_FLAG_EV_FALLING;
    fd_call_ioctl_ptr(gpio_fd_ctx, GPIO_IOCTL_CONFIGURE, &gpio_config);


    create_kernel_task(0x10000, kernel_gpio_irq_thread, gpio_fd_ctx, "gpio-test");

    k_create_socket_t udp_create_socket = {
        .socket_type = SYSCALL_SOCKET_UDP4,
        .udp4.dest_ip.d = {192,168,0,207},
        .udp4.source_port = 0,
        .udp4.dest_port = 69
    };
    int64_t tftp_udp_fd = net_udp_create_socket(&udp_create_socket);
    ASSERT(tftp_udp_fd >= 0);

    void* tftp_ctx = tftp_create_client(tftp_udp_fd);

    void* file_data;
    int64_t file_len;
    int64_t ok = tftp_recv_file(tftp_ctx, "/disk_image.img", &file_data, &file_len);
    ASSERT(ok == 0);

    console_log(LOG_INFO, "Read file %d", file_len);

    loopback_create_raw_device(file_data, file_len, "loop0");

    ok = fs_manager_mount_device("sys", "loop0", FS_TYPE_EXT2, "home");

    if (ok == 0) {
        uint64_t echo_tid;
        char* echo_argv[] = {
            "Echo in userspace",
            NULL
        };
        echo_tid = exec_user_task("home", "bin/hello_rust.elf", "hello_rust", echo_argv);
        (void)echo_tid;

        uint64_t cat_tid;
        char* cat_argv[] = {
            "home",
            "hello.txt",
            NULL
        };
        cat_tid = exec_user_task("home", "bin/cat.elf", "cat", cat_argv);
        (void)cat_tid;
    } else {
        console_log(LOG_DEBUG, "Mount failed");
    }

    k_gpio_level_t gpio_level = {
        .gpio_num = 42,
        .level = 1
    };

    uint64_t ticknum = 0;
    while (1) {
        task_wait_timer_in(1000*1000);

        ticknum++;

        gpio_level.gpio_num = 42;
        fd_call_ioctl_ptr(gpio_fd_ctx, GPIO_IOCTL_CONFIGURE, &gpio_level);

        gpio_level.gpio_num = 2;
        fd_call_ioctl_ptr(gpio_fd_ctx, GPIO_IOCTL_CONFIGURE, &gpio_level);

        gpio_level.level = !gpio_level.level;
    }


}
