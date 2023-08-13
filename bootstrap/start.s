
.arch armv8-a
.cpu cortex-a57

.align 8

.section .bootstrap.text

.global _bootstrap_start

.extern _stack_base
.extern main_bootstrap

_bootstrap_start:

    mrs x1, MPIDR_EL1
    and x1, x1, 0x3
    cmp x1, 0
    b.ne cpu_jail

    mrs x1, CurrentEL
    cmp x1, 0xC
    b.eq el3_start

    cmp x1, 0x8
    b.eq el2_start

    b el1_start

cpu_jail:
    wfi
    b cpu_jail

el1_start:
    ldr x0, =_stack_base
    and x0, x0, 0xFFFFFFFF
    mov sp, x0

    b main_bootstrap

.loop:
    b .loop

el3_start:
    // Setup EL3 exception vectors
    ldr x2, =el3_exception_vectors
    msr VBAR_EL3, x2

    // Setup EL3 SCR
    //  - RW bit (Lower levels AARCH64)
    //  - NS bit (Lower levels Non-secure)
    mov x2, 0x401
    msr SCR_EL3, x2

    // Setup return state
    mrs x2, ID_AA64PFR0_EL1
    and x2, x2, 0xF00
    cmp x2, 0

    // EL2 exists
    b.ne el2_enter_from_el3

    // EL2 does not exist. Jump to EL1
    b el1_enter_from_el3


el2_enter_from_el3:
    // Setup return address
    ldr x2, =el2_start
    msr ELR_EL3, x2

    // Setup return state. EL2h
    mov x2, 0x9
    msr SPSR_EL3, x2

    // Return to EL2
    eret

el2_start:
    // Setup EL2 exception vectors
    ldr x2, =el2_exception_vectors
    msr VBAR_EL2, x2

    // Setup HCR
    //  - RW bit (Lower levels are AARCH64)
    mov x2, 0x80000000
    msr HCR_EL2, x2

    // Enter EL1
    ldr x2, =el1_start
    msr ELR_EL2, x2

    // Setup return state. EL1h
    mov x2, 0x5
    msr SPSR_EL2, x2

    // Return to EL1
    eret

el1_enter_from_el3:

    // Setup return address
    ldr x2, =el1_start
    msr ELR_EL3, x2

    // Setup return state. EL1h
    mov x2, 0x5
    msr SPSR_EL3, x2

    // Return to EL1
    eret

.macro el3_el2_exception_generic name
.align 7
\name:
    b \name
.endm

.align 11
el3_exception_vectors:
el3_el2_exception_generic el3_sp_0_sync
el3_el2_exception_generic el3_sp_0_irq
el3_el2_exception_generic el3_sp_0_fiq
el3_el2_exception_generic el3_sp_0_serror

el3_el2_exception_generic el3_sp_X_sync
el3_el2_exception_generic el3_sp_X_irq
el3_el2_exception_generic el3_sp_X_fiq
el3_el2_exception_generic el3_sp_X_serror

el3_el2_exception_generic el3_lower_64_sync
el3_el2_exception_generic el3_lower_64_irq
el3_el2_exception_generic el3_lower_64_fiq
el3_el2_exception_generic el3_lower_64_serror

el3_el2_exception_generic el3_lower_32_sync
el3_el2_exception_generic el3_lower_32_irq
el3_el2_exception_generic el3_lower_32_fiq
el3_el2_exception_generic el3_lower_32_serror

.align 11
el2_exception_vectors:
el3_el2_exception_generic el2_sp_0_sync
el3_el2_exception_generic el2_sp_0_irq
el3_el2_exception_generic el2_sp_0_fiq
el3_el2_exception_generic el2_sp_0_serror

el3_el2_exception_generic el2_sp_X_sync
el3_el2_exception_generic el2_sp_X_irq
el3_el2_exception_generic el2_sp_X_fiq
el3_el2_exception_generic el2_sp_X_serror

el3_el2_exception_generic el2_lower_64_sync
el3_el2_exception_generic el2_lower_64_irq
el3_el2_exception_generic el2_lower_64_fiq
el3_el2_exception_generic el2_lower_64_serror

el3_el2_exception_generic el2_lower_32_sync
el3_el2_exception_generic el2_lower_32_irq
el3_el2_exception_generic el2_lower_32_fiq
el3_el2_exception_generic el2_lower_32_serror

.section .bootstrap.data

.bss
