
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stdlib/bitutils.h"

#include "drivers/pl011_uart.h"
#include "drivers/qemu_fw_cfg.h"

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

#include "kernel/lib/vmalloc.h"

#include "include/k_ioctl_common.h"

#include "drivers/aarch64/aarch64.h"

void kernel_init_lower_thread(void* ctx);

void main() {

    gtimer_early_init();

    kmalloc_init();

    pl011_init(VIRT_UART);

    pl011_puts(VIRT_UART, "Hello World!\n");

    assert_aarch64_support();

    pagefault_init();
    exception_init();

    vmalloc_init(16 * 1024 * 1024);

    memspace_init_kernelspace();
    memspace_init_systemspace();
    memory_entry_device_t virtuart_device = {
       .start = (uint64_t)VIRT_UART_VMEM,
       .end = (uint64_t)VIRT_UART_VMEM + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)VIRT_UART
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&virtuart_device);

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

    console_log(LOG_DEBUG, "UART_VMEM is Mapped\n");

    syscall_init();
    interrupt_init();

    sys_device_init();

    drivers_init();

    task_init((uint64_t*)exstack_entry.base);
    create_kernel_task(8192, kernel_init_lower_thread, NULL);

    schedule();

    PANIC("Schedule Returned");
    while (1);


    // modules_init_list();
    // modules_start();

    // schedule();

    // console_write("Should not get here\n");

    // gic_set_irq_handler(timer_handler, 30);

    // gic_init();
    // gic_enable_intid(30);
    // gic_enable();


    // while (1) {

    //     timer_irq_fired = 0;

    //     gtimer_start_downtimer(62500000, true);

    //     while (timer_irq_fired == 0) {
    //     }

    //     console_write("Tiggered\n");
    // }
}

void kernel_init_lower_thread(void* ctx) {

    ext2_register();
    sysfs_register();

    dtb_init();

    driver_run_late_init();

    gtimer_init();
    pl011_init_rx(VIRT_UART_VMEM);
    interrupt_enable();

    int64_t open_res;
    open_res = fs_manager_mount_device("sys", "virtio_disk0", FS_TYPE_EXT2,
                                       "home");
    ASSERT(open_res >= 0);

    uint64_t cat_tid;
    char* cat_argv[] = {
        "home",
        "hello.txt",
        NULL
    };
    cat_tid = exec_user_task("home", "bin/cat.elf", "cat", cat_argv);
    (void)cat_tid;

    uint64_t gsh_tid;
    char* gsh_argv[] = {
        NULL
    };
    gsh_tid = exec_user_task("home", "bin/gsh.elf", "gsh", gsh_argv);
    (void)gsh_tid;

    uint64_t echo_tid;
    char* echo_argv[] = {
        "This is in echo!\n",
        NULL
    };
    echo_tid = exec_user_task("home", "bin/echo.elf", "echo", echo_argv);
    (void)echo_tid;

    uint64_t cat2_tid;
    char* cat2_argv[] = {
        "sysfs",
        "tasks",
        NULL
    };
    cat2_tid = exec_user_task("home", "bin/cat.elf", "cat", cat2_argv);
    (void)cat2_tid;

    console_printf("Starting Timer\n");

    uint64_t freq = gtimer_get_frequency();
    uint64_t ticknum = 0;
    while (1) {
        gtimer_start_downtimer(freq, true);
        gtimer_wait_for_trigger();
        // while (!gtimer_downtimer_triggered()) {}
        //console_printf("Tick %d\n", ticknum);
        ticknum++;
    }

    while (1) {
        asm volatile ("svc #0");
    }
}
