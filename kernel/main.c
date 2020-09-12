
#include <stdint.h>
#include <stdbool.h>

#include "drivers/pl011_uart.h"

#include "kernel/console.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "kernel/exception.h"
#include "kernel/vmem.h"


void main_bootstrap() {

    _vmem_table* vmem_l0_table = vmem_create_kernel_map();

    _vmem_ap_flags ap_flags = VMEM_AP_P_R |
                              VMEM_AP_P_W |
                              VMEM_AP_P_E;

    vmem_print_l0_table(vmem_l0_table);

    vmem_set_l0_table(vmem_l0_table);
    vmem_initialize();
    vmem_enable_translations();

    main_preamble();

    while (1); // Should not reach
} __attribute__((section ("bootstrap")));

void main() {

    kmalloc_init();

    pl011_init(VIRT_UART);

    pl011_puts(VIRT_UART, "Hello World!\n");

    exception_init();

    _vmem_table* vmem_l0_table = vmem_create_kernel_map();

    _vmem_ap_flags ap_flags = VMEM_AP_P_R |
                              VMEM_AP_P_W |
                              VMEM_AP_P_E;

    vmem_map_address(vmem_l0_table, (addr_phy_t)VIRT_UART, (addr_virt_t)VIRT_UART, ap_flags, VMEM_ATTR_DEVICE);
    vmem_map_address(vmem_l0_table, (addr_phy_t)VIRT_UART, (addr_virt_t)VIRT_UART_VMEM, ap_flags, VMEM_ATTR_DEVICE);

    vmem_print_l0_table(vmem_l0_table);

    vmem_set_l0_table(vmem_l0_table);
    vmem_initialize();
    vmem_enable_translations();

    pl011_puts(VIRT_UART, "UART is Mapped!\n");
    pl011_puts(VIRT_UART_VMEM, "UART_VMEM is Mapped!\n");
    
    while (1) {
    }
    //asm(".word 0");
}
