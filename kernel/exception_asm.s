

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
    b \impl
.rept 31
.word 0
.endr
.endm

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

generic_exception curr_el_sp_0_sync exception_handler_sync
generic_exception curr_el_sp_0_irq exception_handler_irq
dummy_exception curr_el_sp_0_fiq 2
dummy_exception curr_el_sp_0_serror 3

generic_exception curr_el_sp_X_sync exception_handler_sync
generic_exception curr_el_sp_X_irq exception_handler_irq
dummy_exception curr_el_sp_X_fiq 6
dummy_exception curr_el_sp_X_serror 7

generic_exception curr_el_sp_64_sync exception_handler_sync_lower
generic_exception curr_el_sp_64_irq exception_handler_irq_lower
dummy_exception lower_el_64_fiq 10
dummy_exception lower_el_64_serror 11

dummy_exception lower_el_32_sync 12
dummy_exception lower_el_32_irq 13
dummy_exception lower_el_32_fiq 14
dummy_exception lower_el_32_serror 15
