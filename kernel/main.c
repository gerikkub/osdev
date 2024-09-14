
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "kernel/exception.h"
#include "kernel/vmem.h"
#include "kernel/pagefault.h"
#include "kernel/gtimer.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/task.h"
#include "kernel/schedule.h"
#include "kernel/syscall.h"
#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/elf.h"
#include "kernel/drivers.h"
#include "kernel/sys_device.h"
#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/sysfs/sysfs.h"
#include "kernel/fs/ramfs/ramfs.h"

#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/tcp_conn.h"
#include "kernel/net/tcp_socket.h"
#include "kernel/net/tcp_bind.h"

#include "kernel/lib/vmalloc.h"

#include "drivers/aarch64/aarch64.h"

#include "board_conf_generic.h"

void write32(void* ptr, uint32_t val) {
    *(volatile uint32_t*)ptr = val;
    asm volatile ("dmb SY");
}

uint32_t read32(void* ptr) {
    asm volatile ("dmb SY");
    return *(volatile uint32_t*)ptr;
}

static uint64_t dummy_val;

void kernel_init_lower_thread(void* ctx);

void main() {

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
    //vmem_initialize();

    board_init_early_console();

    console_log(LOG_DEBUG, "Hello!\n");
    vmem_flush_tlb();

    assert_aarch64_support();

    schedule_init();
    syscall_init();
    interrupt_init();

    sys_device_init();

    drivers_init();

    net_init();
    net_arp_init();
    net_arp_table_init();
    net_ipv4_init();
    net_route_init();
    net_tcp_conn_init();
    net_tcp_socket_init();
    net_tcp_bind_init();

    ext2_register();
    sysfs_register();
    ramfs_register();

    board_init_devices();

    gtimer_init();

    // Enable FP support
    uint64_t cpacr = 0x300000;
    WRITE_SYS_REG(CPACR_EL1, cpacr);

    DISABLE_IRQ();

    task_init((uint64_t*)exstack_entry.base);
    create_kernel_task(0x10000, kernel_init_lower_thread, &dummy_val, "kernel");

    schedule();

    PANIC("Schedule Returned");
    while (1);
}

void kernel_init_lower_thread(void* ctx) {

    console_log(LOG_INFO, "Kernel Main Lower");

    ASSERT(ctx == &dummy_val)

    driver_run_late_init();
    interrupt_enable();

    board_discover_devices();

    net_start_task();
    net_tcp_conn_start_timeout_thread();

    board_loop();

    while (1) {
        asm volatile ("svc #0");
    }
}
