

.arch armv8-a

.text

.global exception_init

.macro dummy_exception name num
\name :
    mov x0, #\num
    b unhandled_exception
.rept 30
.word 0
.endr
.endm

.macro generic_exception name impl
\name :
    stp x9, x10, [sp], #-16
    stp x11, x12, [sp], #-16
    stp x13, x14, [sp], #-16
    stp x15, xzr, [sp], #-16
    bl \impl
    ldp x15, xzr, [sp, #0]!
    ldp x13, x14, [sp, #0]!
    ldp x11, x12, [sp, #0]!
    ldp x9, x10, [sp, #0]!
    eret
.rept 22
.word 0
.endr
.endm

.macro save_context_asm name spreg num func
\name:
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    stp xzr, x0, [sp, #-16]!
    add x0, sp, #8
    mrs x1, \spreg
    mrs x2, ELR_EL1
    mrs x3, SPSR_EL1
    mov x4, #\num
    ldr x5, =\func
    b save_context
.rept 11
.word 0
.endr
.endm

.global restore_context_asm

restore_context_asm:
    mov x30, x0
    msr ELR_EL1, x1
    msr SPSR_EL1, x2
    ldp x0, x1, [x30], #16
    ldp x2, x3, [x30], #16
    ldp x4, x5, [x30], #16
    ldp x6, x7, [x30], #16
    ldp x8, x9, [x30], #16
    ldp x10, x11, [x30], #16
    ldp x12, x13, [x30], #16
    ldp x14, x15, [x30], #16
    ldp x16, x17, [x30], #16
    ldp x18, x19, [x30], #16
    ldp x20, x21, [x30], #16
    ldp x22, x23, [x30], #16
    ldp x24, x25, [x30], #16
    ldp x26, x27, [x30], #16
    ldp x28, x29, [x30], #16
    ldr x30, [x30]
    eret


exception_filename:
.string "exception.s"

exception_msg:
.string "Unhandled Exception"

default_vector:
    ldr x0, =exception_filename
    mov x1, xzr
    ldr x2, =exception_msg

    bl panic

exception_init:
    ldr x0, =_exception_vector_base
    msr VBAR_EL1, x0

    msr DAIF, xzr

    ret

.section .exception_vector

save_context_asm curr_el_sp_0_sync SP_EL0 0 exception_handler_sync
save_context_asm curr_el_sp_0_irq SP_EL0 1 exception_handler_irq
dummy_exception curr_el_sp_0_fiq 2
dummy_exception curr_el_sp_0_serror 3

generic_exception curr_el_sp_X_sync exception_handler_sync
generic_exception curr_el_sp_X_irq exception_handler_irq
dummy_exception curr_el_sp_X_fiq 6
dummy_exception curr_el_sp_X_serror 7

save_context_asm curr_el_sp_64_sync SP_EL0 8 exception_handler_sync
save_context_asm curr_el_sp_64_irq SP_EL0 9 exception_handler_irq
dummy_exception lower_el_64_fiq 10
dummy_exception lower_el_64_serror 11

dummy_exception lower_el_32_sync 12
dummy_exception lower_el_32_irq 13
dummy_exception lower_el_32_fiq 14
dummy_exception lower_el_32_serror 15
