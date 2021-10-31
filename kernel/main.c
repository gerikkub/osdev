
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
#include "kernel/gic.h"
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
#include "kernel/fs/ext2.h"

#include "kernel/lib/vmalloc.h"

#include "include/k_fs_ioctl_common.h"

void kernel_init_lower_thread(void* ctx);

void main() {

    kmalloc_init();

    pl011_init(VIRT_UART);

    pl011_puts(VIRT_UART, "Hello World!\n");

    pagefault_init();
    exception_init();

    memspace_init_kernelspace();
    memspace_init_systemspace();
    memory_entry_device_t virtuart_device = {
       .start = (uint64_t)VIRT_UART_VMEM,
       .end = (uint64_t)VIRT_UART_VMEM + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)VIRT_UART
    };

    memory_entry_device_t gicd_device = {
       .start = (uint64_t)GICD_BASE_VIRT,
       .end = (uint64_t)GICD_BASE_VIRT + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)GICD_BASE_PHYS
    };

    memory_entry_device_t gicc_device = {
       .start = (uint64_t)GICC_BASE_VIRT,
       .end = (uint64_t)GICC_BASE_VIRT + VMEM_PAGE_SIZE,
       .type = MEMSPACE_DEVICE,
       .flags = MEMSPACE_FLAG_PERM_KRW,
       .phy_addr = (uint64_t)GICC_BASE_PHYS
    };

    memory_entry_device_t qemu_fw_cfg_device = {
        .start = (uint64_t)QEMU_FW_CFG_VIRT,
        .end = (uint64_t)QEMU_FW_CFG_VIRT + VMEM_PAGE_SIZE,
        .type = MEMSPACE_DEVICE,
        .flags = MEMSPACE_FLAG_PERM_KRW,
        .phy_addr = (uint64_t)QEMU_FW_CFG_PHY
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&virtuart_device);
    memspace_add_entry_to_kernel_memory((memory_entry_t*)&gicd_device);
    memspace_add_entry_to_kernel_memory((memory_entry_t*)&gicc_device);
    memspace_add_entry_to_kernel_memory((memory_entry_t*)&qemu_fw_cfg_device);

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

    vmalloc_init(16 * 1024 * 1024);

    console_log(LOG_DEBUG, "UART_VMEM is Mapped\n");

    gtimer_init();
    syscall_init();

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

    sys_device_init();

    drivers_init();

    ext2_register();

    dtb_init();

    fs_manager_mount_device("sys", "virtio_disk0", FS_TYPE_EXT2,
                            "home");

    fd_ops_t file_ops;
    void* file_ctx;

    vfs_open_device("home", "bin/cat.elf", 0, &file_ops, &file_ctx);

    int64_t cat_size = file_ops.ioctl(file_ctx, FS_IOCTL_FSIZE, NULL, 0);
    ASSERT(cat_size > 0);

    uint8_t* read_buffer = vmalloc(cat_size);
    memset(read_buffer, 0, cat_size);

    int64_t size = file_ops.read(file_ctx, read_buffer, cat_size, 0);
    ASSERT(size == cat_size);

    uint64_t cat_tid;
    elf_result_t res;
    cat_tid = create_elf_task(read_buffer, size, &res, false, "cat");
    (void)cat_tid;

    ASSERT(res == ELF_VALID);

    while (1) {
        asm volatile ("svc #0");
    }
}
