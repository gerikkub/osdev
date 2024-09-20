
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

    ldr x0, =el1_bootstrap_exception_vectors
    msr VBAR_EL1, x0

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
    //  - FPEN bit (Float Point Enabled)
    mov x2, 0x80000000
    msr HCR_EL2, x2

    // Setup CPTR_EL2
    //  - FPEN bit (Float Point Enabled)
    mov x2, 0x00300000
    msr CPTR_EL2, x2

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


write_byte:
    str x1, [x0]
    mov x20, #10000
1:
    sub x20, x20, #1
    bne 1b
    ret

write_nibble:
    cmp x1, #10
    bgt 1f
    add x1, x1, '0'
    b 1f

    sub x1, x1, #10
    add x1, x1, 'a'
    b write_byte
1:
    add x1, x1, '0'
    b write_byte


write_u64:
    mov x2, x1
    mov x3, #16
    mov x4, lr

1:
    lsr x1, x2, #60
    bl write_nibble
    lsl x2, x2, #4
    sub x3, x3, #1
    cbnz x3, 1b

    mov lr, x4
    ret

.extern bootstrap_write_byte
.extern bootstrap_write_word

.macro el1_exception_generic name num
.align 7
\name:
// Disable MMU
    mrs x0, SCTLR_EL1
    bic x0, x0, #1
    msr SCTLR_EL1, x0

    mov x0, 'E
    bl bootstrap_write_byte

    mov x0, '\n
    bl bootstrap_write_byte

    mrs x0, ESR_EL1
    bl bootstrap_write_word

    mov x0, '\n
    bl bootstrap_write_byte

    mov x0, 'F
    bl bootstrap_write_byte

    mrs x0, FAR_EL1
    bl bootstrap_write_word

    mov x0, '\n
    bl bootstrap_write_byte

    mov x0, 'L
    bl bootstrap_write_byte

    mrs x0, ELR_EL1
    bl bootstrap_write_word


1:
    b 1b

.endm


.macro el3_el2_exception_generic name num
.align 7
\name:
1:
    b 1b

.endm

.align 11
el3_exception_vectors:
el3_el2_exception_generic el3_sp_0_sync 0
el3_el2_exception_generic el3_sp_0_irq 1
el3_el2_exception_generic el3_sp_0_fiq 2
el3_el2_exception_generic el3_sp_0_serror 3

el3_el2_exception_generic el3_sp_X_sync 4
el3_el2_exception_generic el3_sp_X_irq 5
el3_el2_exception_generic el3_sp_X_fiq 6
el3_el2_exception_generic el3_sp_X_serror 7

el3_el2_exception_generic el3_lower_64_sync 8
el3_el2_exception_generic el3_lower_64_irq 9
el3_el2_exception_generic el3_lower_64_fiq 10
el3_el2_exception_generic el3_lower_64_serror 11

el3_el2_exception_generic el3_lower_32_sync 12
el3_el2_exception_generic el3_lower_32_irq 13
el3_el2_exception_generic el3_lower_32_fiq 14
el3_el2_exception_generic el3_lower_32_serror 15

.align 11
el2_exception_vectors:
el3_el2_exception_generic el2_sp_0_sync 0
el3_el2_exception_generic el2_sp_0_irq 1
el3_el2_exception_generic el2_sp_0_fiq 2
el3_el2_exception_generic el2_sp_0_serror 3

el3_el2_exception_generic el2_sp_X_sync 4
el3_el2_exception_generic el2_sp_X_irq 5
el3_el2_exception_generic el2_sp_X_fiq 6
el3_el2_exception_generic el2_sp_X_serror 7

el3_el2_exception_generic el2_lower_64_sync 8
el3_el2_exception_generic el2_lower_64_irq 9
el3_el2_exception_generic el2_lower_64_fiq 10
el3_el2_exception_generic el2_lower_64_serror 11

el3_el2_exception_generic el2_lower_32_sync 12
el3_el2_exception_generic el2_lower_32_irq 13
el3_el2_exception_generic el2_lower_32_fiq 14
el3_el2_exception_generic el2_lower_32_serror 15

.align 11
el1_bootstrap_exception_vectors:
el1_exception_generic el1_bootstrap_sp_0_sync 0
el1_exception_generic el1_bootstrap_sp_0_irq 1
el1_exception_generic el1_bootstrap_sp_0_fiq 2
el1_exception_generic el1_bootstrap_sp_0_serror 3

el1_exception_generic el1_bootstrap_sp_X_sync 4
el1_exception_generic el1_bootstrap_sp_X_irq 5
el1_exception_generic el1_bootstrap_sp_X_fiq 6
el1_exception_generic el1_bootstrap_sp_X_serror 7

el1_exception_generic el1_bootstrap_lower_64_sync 8
el1_exception_generic el1_bootstrap_lower_64_irq 9
el1_exception_generic el1_bootstrap_lower_64_fiq 10
el1_exception_generic el1_bootstrap_lower_64_serror 11

el1_exception_generic el1_bootstrap_lower_32_sync 12
el1_exception_generic el1_bootstrap_lower_32_irq 13
el1_exception_generic el1_bootstrap_lower_32_fiq 14
el1_exception_generic el1_bootstrap_lower_32_serror 15
.section .bootstrap.data

.bss
