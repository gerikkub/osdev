
.arch armv8-a
.cpu cortex-a57

.align 8

.section .bootstrap.text

.global _start

.extern _stack_base
.extern main_bootstrap

_start:

    ldr x0, =_stack_base
    and x0, x0, 0xFFFFFFFF
    mov sp, x0

    b main_bootstrap

.loop:
    b .loop

.section .bootstrap.data

dummy:
.word 0xA5A5A5A5

.bss

stack_base:

stack_top:
