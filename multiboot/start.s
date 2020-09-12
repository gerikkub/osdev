
.arch armv8-a
.cpu cortex-a57

.align 8

.text

.global _start

.extern _stack_base

_start:

    ldr x0, =_stack_base
    mov sp, x0

    b main

.loop:
    b .loop

.data

dummy:
.word 0xA5A5A5A5

.bss

stack_base:

stack_top:
