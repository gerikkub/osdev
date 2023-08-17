
.arch armv8-a
.cpu cortex-a57

.align 8

.section .text

.global _high_start

.extern main

_high_start:

    // // Set SP_EL1 to the exception stack
    // mrs x0, SPSel
    // orr x0, x0, #1
    // msr SPSel, x0

    // ldr x0, =_exception_stack_base
    // mov sp, x0

    // // Setup kernel to use SP_EL0
    // mrs x0, SPSel
    // // Clear the SPSel bit
    // mov x1, xzr
    // orr x1, x1, #1
    // mvn x1, x1
    // and x0, x0, x1
    // msr SPSel, x0

    ldr x0, =_stack_base
    mov sp, x0

    ldr x0, =_bss_start
    ldr x1, =_bss_end
    sub x2, x1, x0
    mov x1, 0
    bl memset

    b main

.loop:
    b .loop

