
.arch armv8-a
.cpu cortex-a57

.align 8

.section .text

.global _high_start

.extern main

_high_start:

    ldr x0, =_stack_base
    mov sp, x0

    b main

.loop:
    b .loop

