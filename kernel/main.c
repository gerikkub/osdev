
#include <stdint.h>
#include <stdbool.h>

#include "drivers/pl011_uart.h"

#include "kernel/console.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "kernel/exception.h"
#include "kernel/vmem.h"
#include "kernel/pagefault.h"
#include "kernel/gtimer.h"
#include "kernel/gic.h"


static volatile uint32_t dummy_mem = 1;

extern volatile uint8_t gic_got_irq;

void main() {

    kmalloc_init();

    pl011_init(VIRT_UART);

    pl011_puts(VIRT_UART, "Hello World!\n");

    pagefault_init();
    exception_init();

    _vmem_table* vmem_l0_table = vmem_create_kernel_map();

    _vmem_ap_flags ap_flags = VMEM_AP_P_R |
                              VMEM_AP_P_W |
                              VMEM_AP_P_E;

    //vmem_map_address(vmem_l0_table, (addr_phy_t)VIRT_UART, (addr_virt_t)VIRT_UART, ap_flags, VMEM_ATTR_DEVICE);
    vmem_map_address(vmem_l0_table, (addr_phy_t)VIRT_UART, (addr_virt_t)VIRT_UART_VMEM, ap_flags, VMEM_ATTR_DEVICE);
    vmem_map_address(vmem_l0_table, (addr_phy_t)GICD_BASE_PHYS, (addr_virt_t)GICD_BASE_VIRT, ap_flags, VMEM_ATTR_DEVICE);
    vmem_map_address(vmem_l0_table, (addr_phy_t)GICC_BASE_PHYS, (addr_virt_t)GICC_BASE_VIRT, ap_flags, VMEM_ATTR_DEVICE);

    _vmem_table* dummy_user_table = vmem_allocate_empty_table();

    vmem_set_tables(vmem_l0_table, dummy_user_table);

    pl011_puts(VIRT_UART_VMEM, "UART_VMEM is Mapped!\n");

    gtimer_init();

    gic_init();
    gic_enable_intid(30);
    gic_enable();


    while (1) {

        gtimer_start_downtimer(62500000, true);

        while (gic_got_irq == 0) {
        }

        console_write("Tiggered\n");

        gic_got_irq = 0;
    }
}
