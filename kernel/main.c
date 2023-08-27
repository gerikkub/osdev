
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

#include "drivers/pl011_uart.h"
#include "drivers/qemu_fw_cfg.h"
#include "drivers/virtio_pci_early_console/virtio_pci_early_console.h"

#include "kernel/console.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "kernel/exception.h"
#include "kernel/vmem.h"
#include "kernel/pagefault.h"
#include "kernel/gtimer.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/task.h"
#include "kernel/syscall.h"
#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/modules.h"
#include "kernel/elf.h"
#include "kernel/dtb.h"
#include "kernel/drivers.h"
#include "kernel/vfs.h"
#include "kernel/sys_device.h"
#include "kernel/fs_manager.h"
#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/sysfs/sysfs.h"
#include "kernel/exec.h"

#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/udp.h"
#include "kernel/net/tcp_conn.h"
#include "kernel/net/tcp_socket.h"
#include "kernel/net/tcp_bind.h"

#include "kernel/lib/vmalloc.h"
#include "kernel/lib/libpci.h"

#include "include/k_ioctl_common.h"

#include "drivers/aarch64/aarch64.h"

#include "board_conf_generic.h"

#include "drivers/timer/bcm2835_systemtimer.h"
#include "drivers/gicv2/gicv2.h"


void write32(void* ptr, uint32_t val) {
    *(volatile uint32_t*)ptr = val;
    asm volatile ("dmb SY");
}

uint32_t read32(void* ptr) {
    asm volatile ("dmb SY");
    return *(volatile uint32_t*)ptr;
}

void kernel_init_lower_thread(void* ctx);

void main() {

    volatile uint8_t* uart_tx_ptr = (volatile uint8_t*)0x47E215040;
    *uart_tx_ptr = 'A';
    
    board_init_main_early();

    console_write("Kernel Main\n");

    gtimer_early_init();

    kmalloc_init();

    pagefault_init();
    exception_init();

    vmalloc_init(16 * 1024 * 1024);

    memspace_init_kernelspace();
    memspace_init_systemspace();

    board_init_mappings();

    uint64_t* exstack_mem = kmalloc_phy(KSPACE_EXSTACK_SIZE);
    ASSERT(exstack_mem != NULL);

    memory_entry_stack_t exstack_entry = {
        .start = 0xFFFF800000000000,
        .end = 0xFFFF800000000000 + KSPACE_EXSTACK_SIZE + (2*USER_STACK_GUARD_PAGES),
        .type = MEMSPACE_STACK,
        .flags = MEMSPACE_FLAG_PERM_KRW,
        .phy_addr = (uint64_t)exstack_mem,
        .base = 0xFFFF800000000000 + KSPACE_EXSTACK_SIZE + USER_STACK_GUARD_PAGES,
        .limit = 0xFFFF800000000000 + USER_STACK_GUARD_PAGES,
        .maxlimit = 0xFFFF800000000000 + USER_STACK_GUARD_PAGES,
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&exstack_entry);

    _vmem_table* kernel_vmem_table = memspace_build_kernel_vmem();

    _vmem_table* dummy_user_table = vmem_allocate_empty_table();

    vmem_set_tables(kernel_vmem_table, dummy_user_table);

    board_init_early_console();

    console_log(LOG_DEBUG, "Hello!\n");

    assert_aarch64_support();

    syscall_init();
    interrupt_init();

    sys_device_init();

    drivers_init();

    net_arp_init();
    net_arp_table_init();
    net_ipv4_init();
    net_route_init();
    net_tcp_conn_init();
    net_tcp_socket_init();
    net_tcp_bind_init();

    ext2_register();
    sysfs_register();

    board_init_devices();

    gtimer_init();

    DISABLE_IRQ();

    task_init((uint64_t*)exstack_entry.base);
    create_kernel_task(8192, kernel_init_lower_thread, NULL, "kernel");

    schedule();

    PANIC("Schedule Returned");
    while (1);
}

void kernel_init_lower_thread(void* ctx) {

    driver_run_late_init();
    interrupt_enable();

    net_tcp_conn_start_timeout_thread();

    /*
    int64_t open_res;
    open_res = fs_manager_mount_device("sys", "virtio_disk0", FS_TYPE_EXT2,
                                       "home");
    ASSERT(open_res >= 0);

    fd_ops_t nic_ops;
    void* nic_ctx;
    open_res = vfs_open_device("sys",
                               "virtio-pci-net0",
                               0,
                               &nic_ops,
                               &nic_ctx,
                               NULL);
    if (open_res < 0) {
        console_log(LOG_INFO, "virtio-pci-net0 not available. Skipping IP set");
    } else {
        uint64_t ip = 10 << 24 |
                      0 << 16 |
                      2 << 8 |
                      15;
        int64_t res;
        res = nic_ops.ioctl(nic_ctx, NET_IOCTL_SET_IP, &ip, 1);
        ASSERT(res == 0);

        uint64_t ip_net = 10 << 24 |
                          0 << 16 |
                          2 << 8 |
                          0;

        uint64_t args[2] = {
            ip_net, 24
        };

        res = nic_ops.ioctl(nic_ctx, NET_IOCTL_SET_ROUTE, args, 2);

        uint64_t ip_route = 10 << 24 |
                            0 << 16 |
                            2 << 8 |
                            1;

        uint64_t args_default[2] = {
            ip_route, 24
        };
        res = nic_ops.ioctl(nic_ctx, NET_IOCTL_SET_DEFAULT_ROUTE, args_default, 2);
    }
    */

    /*
    uint64_t addline_tid;
    char* addline_argv[] = {
        "home",
        "hello.txt",
        "newline",
        NULL
    };
    addline_tid = exec_user_task("home", "bin/addline.elf", "newline", addline_argv);
    (void)addline_tid;

    uint64_t cat_tid;
    char* cat_argv[] = {
        "home",
        "hello.txt",
        NULL
    };
    cat_tid = exec_user_task("home", "bin/cat.elf", "cat", cat_argv);
    (void)cat_tid;

    uint64_t touch_tid;
    char* touch_argv[] = {
        "home",
        "new.txt",
        "This is a new file!",
        NULL
    };
    touch_tid = exec_user_task("home", "bin/touch.elf", "touch", touch_argv);
    (void)touch_tid;

    uint64_t cat2_tid;
    char* cat2_argv[] = {
        "home",
        "new.txt",
        NULL
    };
    cat2_tid = exec_user_task("home", "bin/cat.elf", "cat", cat2_argv);
    (void)cat2_tid;
    */

    /*
    uint64_t gsh_tid;
    char* gsh_argv[] = {
        NULL
    };
    gsh_tid = exec_user_task("home", "bin/gsh.elf", "gsh", gsh_argv);
    (void)gsh_tid;
    */

   /*
    uint64_t udp_recv_tid;
    char* udp_recv_argv[] = {
        "10.0.2.1",
        "4455",
        NULL
    };
    (void)udp_recv_argv;
    udp_recv_tid = exec_user_task("home", "bin/udp_recv.elf", "udp_recv", udp_recv_argv);
    (void)udp_recv_tid;
    */

   /*
   uint64_t echo_tid;
   char* echo_argv[] = {
        "Hello"
   };
   (void)echo_argv;
   echo_tid = exec_user_task("home", "bin/echo.elf", "echo", echo_argv);
   (void)echo_tid;

    uint64_t tcp_listen_tid;
    char* tcp_listen_argv[] = {
        "10.0.2.15",
        "4455",
        NULL
    };
    (void)tcp_listen_argv;
    tcp_listen_tid = exec_user_task("home", "bin/tcp_listen.elf", "tcp_listen", tcp_listen_argv);
    (void)tcp_listen_tid;

    uint64_t http_server_tid;
    char* http_server_argv[] = {
        "10.0.2.15",
        "8088",
        NULL
    };
    (void)http_server_argv;
    http_server_tid = exec_user_task("home", "bin/http_server.elf", "http_server", http_server_argv);
    (void)http_server_tid;
    */

    uint64_t count_a_tid;
    uint64_t count_b_tid;
    //count_a_tid = exec_user_task("home", "bin/count.elf", "count_a", NULL);
    //count_b_tid = exec_user_task("home", "bin/count.elf", "count_a", NULL);
    (void)count_a_tid;
    (void)count_b_tid;

    /*
    uint64_t tcp_cat_tid;
    char* tcp_cat_argv[] = {
        "10.0.2.15",
        "5555",
        NULL
    };
    (void)tcp_cat_argv;
    tcp_cat_tid = exec_user_task("home", "bin/tcp_cat.elf", "tcp_cat", tcp_cat_argv);
    (void)tcp_cat_tid;
    */

    console_log(LOG_DEBUG, "Starting Timer\n");

    uint64_t freq = gtimer_get_frequency();
    uint64_t ticknum = 0;
    //int64_t lastmem = 0;
    uint64_t tcp_tid = 0;
    while (1) {
        task_wait_timer_in(1000*1000);

        ticknum++;

        console_log(LOG_DEBUG, "Tick %d", ticknum);

        ipv4_t dest_ip = {
            .d = {10, 0, 2, 1}
        };
        (void)dest_ip;

        /*
        net_ipv4_icmp_hdr_t ping_request = {
            .type = NET_IPV4_ICMP_ECHO_REQUEST,
            .code = 0,
            .checksum = 0,
            .echo.id = 1,
            .echo.seq = ticknum,
            .payload = (uint8_t*)"Gerik!",
            .payload_len = 6
        };

        net_ipv4_icmp_send_packet(&dest_ip, &ping_request);
        */

        //net_udp_send_packet(&dest_ip, 5555, 2233, (uint8_t*)"Hello!\n", 7);

        /*
        char buffer[5] = {0};
        char* wait_task_argv[] = {
            buffer,
            NULL
        };
        snprintf(wait_task_argv[0], 5, "%d", echo_tid);
        uint64_t wait_task_tid;
        if (ticknum == 20) {
            wait_task_tid = exec_user_task("home", "bin/wait_task.elf", "wait_task", wait_task_argv);
        }
        (void)wait_task_tid;
        */

        char* tcp_argv[] = {
            "10.0.2.1",
            "6666",
            "test\n",
            "5",
            NULL
        };
        (void)tcp_argv;
        if ((gtimer_get_count() / freq) > 15 && tcp_tid == 0) {
            //tcp_tid = exec_user_task("home", "bin/tcp_test.elf", "tcp_test", tcp_argv);
        }
        (void)tcp_tid;

/*
        uint64_t time_tid;
        time_tid = exec_user_task("home", "bin/time.elf", "time", NULL);
        (void)time_tid;
        */

        uint64_t cat2_tid;
        char* cat2_argv[] = {
            "sysfs",
            "profile",
            NULL
        };
        //cat2_tid = exec_user_task("home", "bin/cat.elf", "cat", cat2_argv);
        (void)cat2_argv;
        (void)cat2_tid;
/*
        uint64_t cat2_tid;
        char* cat2_argv[] = {
            "sysfs",
            "vmalloc_stat",
            NULL
        };
        cat2_tid = exec_user_task("home", "bin/cat.elf", "cat", cat2_argv);
        (void)cat2_tid;

        malloc_stat_t stat;
        int64_t deltamem = 0;
        vmalloc_calc_stat(&stat);
        if (lastmem != 0) {
            deltamem = lastmem - stat.avail_mem;
        }
        lastmem = stat.avail_mem;
        console_printf("%u %u %u %d",
                       stat.total_mem,
                       stat.avail_mem,
                       stat.largest_chunk,
                       deltamem);
        console_printf("\n");
        */
    }

    while (1) {
        asm volatile ("svc #0");
    }
}
