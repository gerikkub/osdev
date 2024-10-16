

.arch armv8-a

.text

.global exception_init
.global switch_to_kernel_task_stack_asm
.global task_wait_kernel_final
.global save_fp_reg
.global restore_fp_reg

.macro dummy_exception name num
.align 7
\name :
    mov x0, #\num
    b unhandled_exception
.endm

.macro generic_exception name impl
.align 7
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
.endm

.macro save_context_asm_user name spreg num func
.align 7
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
    bl save_context
.endm

.macro save_context_asm_user_irq name spreg num func
.align 7
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

    # Arguments to save_context are as follows
    # x0: contact frame pointer
    # x1: This exception number
    # x2: Exception handler to call after saving context
    bl \func

    # Use the return value to determine the next step
    # 0: Return to the caller
    # 1 (or nonzero): Save state and call schedule

    cbnz x0, \name\()_reschedule
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

\name\()_reschedule:

    # x0 will act as the frame pointer to
    # this return frame. 
    add x0, sp, #8

    # Store context information about this exception
    mov x1, #\num
    ldr x2, =schedule

    # Arguments to save_context are as follows
    # x0: contact frame pointer
    # x1: This exception number
    # x2: Exception handler to call after saving context
    bl save_context

.endm

.macro save_context_asm_task_kern_irq name spreg num func
.align 7
\name:
    # Save all registers to this stack frame
    # The zero write will be overriden with pc for a fake stack frame
    stp fp, xzr, [sp, #-16]!
    mov fp, sp

    # Save all registers but x30
    stp lr, xzr, [sp, #-16]!
    bl save_most_reg

    # Store the return address in the zero write above
    mrs x0, ELR_EL1
    mrs x1, SPSR_EL1
    str x0, [fp, #8]
    stp x0, x1, [sp, #-16]!

    # Store context information about this exception
    mov x0, #\num
    mov x1, sp

    # Arguments to save_context are as follows
    # x0: This exception number
    # x1: Stack frame
    bl \func

    # Restore exception link register
    ldp x0, x1, [sp], #16
    msr ELR_EL1, x0
    msr SPSR_EL1, x1

    # Restore all registers but x30
    bl restore_most_reg
    # Restore x30
    ldp lr, xzr, [sp], #16

    # Restore lr and fp
    ldp xzr, fp, [sp], #16
    
    # Return from the IRQ
    eret


.endm

.macro panic_exception_asm name num func
.align 7
# Setup just enough to panic
\name:

    # Setup a valid stack
    ldr x0, =_stack_base
    mov sp, x0
    mrs x1, ELR_EL1
    stp fp, lr, [sp, #-16]!
    mov fp, sp
    stp fp, x1, [sp, #-16]!
    mov fp, sp


    mov x0, #\num

    # Arguments to save_context are as follows
    # x0: This exception number
    bl \func

\name\()_loop:
    b \name\()_loop

.endm


    

.global restore_context_asm

# Restore a task from the state_fp structure
# x0: Pointer to the state_fp structure
# x1: Stack pointer for the task. This will be the stack pointer on the
# x2: Stack pointer for the handler
# x3: Current Stack point
restore_context_asm:

    cbz x3, restore_context_asm_el1t

restore_context_asm_el1h:
    mov sp, x2
    msr SP_EL0, x1

    mov x30, x0
    ldp x3, x1, [x30], #16
    ldr x2, [x30], #8

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

restore_context_asm_el1t:
    # Assume someone else has setup the handler SP
    mov sp, x1

    mov x30, x0
    ldp x3, x1, [x30], #16
    ldr x2, [x30], #8

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

# Save all FP/NEON registers to memory pointed to by x0
save_fp_reg:
    stp q0, q1, [x0], #32
    stp q2, q3, [x0], #32
    stp q4, q5, [x0], #32
    stp q6, q7, [x0], #32
    stp q8, q9, [x0], #32
    stp q10, q11, [x0], #32
    stp q12, q13, [x0], #32
    stp q14, q15, [x0], #32
    stp q16, q17, [x0], #32
    stp q18, q19, [x0], #32
    stp q20, q21, [x0], #32
    stp q22, q23, [x0], #32
    stp q24, q25, [x0], #32
    stp q26, q27, [x0], #32
    stp q28, q29, [x0], #32
    stp q30, q31, [x0], #32

    mrs x1, FPCR
    mrs x2, FPSR
    stp x1, x2, [x0], #16

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

# Save all FP/NEON registers to memory pointed to by x0
restore_fp_reg:
    ldp q0, q1, [x0], #32
    ldp q2, q3, [x0], #32
    ldp q4, q5, [x0], #32
    ldp q6, q7, [x0], #32
    ldp q8, q9, [x0], #32
    ldp q10, q11, [x0], #32
    ldp q12, q13, [x0], #32
    ldp q14, q15, [x0], #32
    ldp q16, q17, [x0], #32
    ldp q18, q19, [x0], #32
    ldp q20, q21, [x0], #32
    ldp q22, q23, [x0], #32
    ldp q24, q25, [x0], #32
    ldp q26, q27, [x0], #32
    ldp q28, q29, [x0], #32
    ldp q30, q31, [x0], #32
    ldp x1, x2, [x0], #16

    msr FPCR, x1
    msr FPSR, x2

    ret

# Switch to the task kernel stack and EL1t
# Call the given function
# x0: vector argument to Function
# x1: Per CPU Stack pointer to reset SP1h
# x2: Task kernel stack pointer
# x3: Function to call from the new stack
switch_to_kernel_task_stack_asm:

    # Set the CPU Stack pointer back to the correct value
    mov sp, x1

    # Setup the stack point value for SP_EL0
    msr SP_EL0, x2
    # Switch the stack pointer
    msr SPSel, xzr

    # Call the function
    blr x3

    # Should not get here
    ret

.global task_wait_kernel_asm

# Wait a task inside a kernel context.
# x0: Task pointer
# x1: Wait Type
# x2: Wait Context Ptr
# x3: Runstate
# x4: Wakeup Function
task_wait_kernel_asm:

    // Setup the frame pointer
    stp lr, fp, [sp, #-16]!
    mov fp, sp

    # Don't need to save registers x0-x7
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x28, x29, [sp, #-16]!
    stp x30, xzr, [sp, #-16]!

    mov x5, sp
    bl task_wait_kernel_final

.global restore_context_kernel_asm

# Restore kernel context after a wait call
# x0: The return value to return from task_wait_kernel
# x1: Kernel stack pointer 
restore_context_kernel_asm:

    mov sp, x1

    # Restore state from task_wait_kernel
    ldp x30, xzr, [sp], #16
    ldp x28, x29, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16

    // Cleanup the frame pointer
    ldp lr, fp, [sp], #16

    ret

.section .exception_vector

# Exception while processing in EL1t
save_context_asm_task_kern_irq curr_el_sp_0_sync SP_EL0 0 exception_handler_sync_kernel
//panic_exception_asm curr_el_sp_0_irq 1 panic_exception_handler
save_context_asm_task_kern_irq curr_el_sp_0_irq SP_EL0 1 exception_handler_irq
dummy_exception curr_el_sp_0_fiq 2
panic_exception_asm curr_el_sp_0_serror 3 panic_exception_handler

# Exception while processing in EL1h (in kernel or irq)
save_context_asm_task_kern_irq curr_el_sp_X_sync SP_EL1 4 exception_handler_sync
save_context_asm_task_kern_irq curr_el_sp_X_irq SP_EL1 5 exception_handler_irq
//panic_exception_asm curr_el_sp_x_irq 1 panic_exception_handler
dummy_exception curr_el_sp_X_fiq 6
panic_exception_asm curr_el_sp_X_serror 7 panic_exception_handler

# Exception while in EL0t
save_context_asm_user lower_el_sp_64_sync SP_EL0 8 exception_handler_sync
save_context_asm_user lower_el_sp_64_irq SP_EL0 9 exception_handler_irq
dummy_exception lower_el_64_fiq 10
panic_exception_asm lower_el_64_serror 11 panic_exception_handler

# Not implementing AARCH32 so all dummy exceptions
dummy_exception lower_el_32_sync 12
dummy_exception lower_el_32_irq 13
dummy_exception lower_el_32_fiq 14
dummy_exception lower_el_32_serror 15
