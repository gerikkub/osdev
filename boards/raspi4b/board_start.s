
.arch armv8-a
.cpu cortex-a57

.align 8

.section .bootstrap.text.first

.global _start
.global _init_reg

.extern memset

_start:
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f
    // We're not on the main core, so hang in an infinite wait loop
1:  wfe
    b       1b
2:  // We're on the main core!

    mov sp, x0
    ldr x0, =_init_reg
    add x0, x0, #8
    mov x1, #7
    str x1, [x0, #8]
    add x1, x1, #16
    stp x2, x3, [x0], #16
    stp x4, x5, [x0], #16
    stp x6, x7, [x0], #16
    stp x8, x9, [x0], #16
    stp x10, x11, [x0], #16
    stp x12, x13, [x0], #16
    stp x14, x15, [x0], #16
    stp x16, x17, [x0], #16
    stp x18, x19, [x0], #16
    stp x20, x21, [x0], #16
    stp x22, x23, [x0], #16
    stp x24, x25, [x0], #16
    stp x26, x27, [x0], #16
    stp x28, x29, [x0], #16
    stp x30, xzr, [x0], #16

    mov x1, sp
    ldr x0, =_init_reg
    str x1, [x0]

    ldr x0, =0xFE215040
    mov x1, 0x47
    str x1, [x0]

    b _bootstrap_start

.section .bootstrap.data.first

.align 8
_init_reg:
    .space 256, 0