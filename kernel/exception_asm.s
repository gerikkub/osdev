

.arch armv8-a

.text

.global exception_init
.global switch_to_kernel_stack_asm

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
    # Save all registers to this stack frame
    stp x30, xzr, [sp, #-16]!
    bl save_most_reg

    # Retrieve the previous stack pointer
    # Return address and process status
    # Save these values to the stack as well
    mrs x1, \spreg
    mrs x2, ELR_EL1
    mrs x3, SPSR_EL1
    stp x1, x2, [sp, #-16]!
    stp xzr, x3, [sp, #-16]!

    # x0 will act as the frame pointer to
    # this return frame. 
    add x0, sp, #8

    # Store context information about this exception
    mov x1, #\num
    ldr x2, =\func
    add x3, sp, #266

    # Arguments to save_context are as follows
    # x0: saved state frame pointer
    # x1: This exception number
    # x2: Exception handler to call after saving context
    # x3: Original kernel stack pointer
    bl save_context
.rept 20
.word 0
.endr
.endm

.macro save_context_asm_irq name spreg num func
# Similar to save_context, but don't automatically
# call save_context. Higher up function may decide
# to call this directly
\name:
    # Save all registers to this stack frame
    stp x30, xzr, [sp, #-16]!
    bl save_most_reg

    # Retrieve the previous stack pointer
    # Return address and process status
    # Save these values to the stack as well
    mrs x1, \spreg
    mrs x2, ELR_EL1
    mrs x3, SPSR_EL1
    stp x1, x2, [sp, #-16]!
    stp xzr, x3, [sp, #-16]!

    # x0 will act as the frame pointer to
    # this return frame. 
    add x0, sp, #8

    # Context about this exception
    mov x1, #\num
    ldr x2, =\func
    add x3, sp, #266

    # Arguments to save_context are as follows
    # x0: contact frame pointer
    # x1: This exception number
    # x2: Exception handler to call after saving context
    # x3: Original kernel stack pointer
    bl \func

    # We have returned with sp the same as it
    # was above. Pop context off the stack and return

    # First pop off system registers
    ldp xzr, x3, [sp], #16
    ldp x1, x2, [sp], #16

    msr SPSR_EL1, x3
    msr ELR_EL1, x2
    msr \spreg, x1

    # Pop off remaining registers
    bl restore_most_reg

    # Pop off the link register
    ldp x30, xzr, [sp], #16

    # Return to previous context
    eret

.rept 12
.word 0
.endr
.endm

.global restore_context_asm

# Restore a task from the state_fp structure
# x0: Pointer to the state_fp structure
restore_context_asm:
    mov x30, x0
    ldp x3, x1, [x30], #16
    ldr x2, [x30], #8

    msr SP_EL0, x1
    msr ELR_EL1, x2
    msr SPSR_EL1, x3

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

# Save all registers except x30 which is used for this method call
save_most_reg:
    stp x28, x29, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
    ret

# Restore all registers except x30 which is used for this method call
restore_most_reg:
    ldp x0, x1, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x28, x29, [sp], #16
    ret


# Switch to the kernel stack and call the given function
# x0: vector argument to Function
# x1: Original cpu kernel stack point to revert
# x2: Task kernel stack pointer
# x3: Function to call at the end
switch_to_kernel_stack_asm:

    mov x2, sp

    # # Save this stack pointer value
    # mov sp, x1
    # # Setup the stack point value for SP_EL0
    # msr SP_EL0, x2
    # # Switch the stack pointer
    # msr SPSel, xzr

    # Call the function
    blr x3

    # Should not get here
    ret

.section .exception_vector

# Exception while processing a kernel task
save_context_asm curr_el_sp_0_sync SP_EL0 0 exception_handler_sync
save_context_asm_irq curr_el_sp_0_irq SP_EL0 1 exception_handler_irq
dummy_exception curr_el_sp_0_fiq 2
dummy_exception curr_el_sp_0_serror 3

# Exception while processing an IRQ
save_context_asm curr_el_sp_X_sync SP_EL1 4 exception_handler_sync
save_context_asm_irq curr_el_sp_X_irq SP_EL1 5 exception_handler_irq
dummy_exception curr_el_sp_X_fiq 6
dummy_exception curr_el_sp_X_serror 7

# Exception while in user space
save_context_asm lower_el_sp_64_sync SP_EL0 8 exception_handler_sync
save_context_asm_irq lower_el_sp_64_irq SP_EL0 9 exception_handler_irq
dummy_exception lower_el_64_fiq 10
dummy_exception lower_el_64_serror 11

# Not implementing AARCH32 so all dummy exceptions
dummy_exception lower_el_32_sync 12
dummy_exception lower_el_32_irq 13
dummy_exception lower_el_32_fiq 14
dummy_exception lower_el_32_serror 15
