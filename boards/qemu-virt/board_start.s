
.arch armv8-a
.cpu cortex-a57

.align 8

.section .bootstrap.text

.global _start

_start:
    b _bootstrap_start
_2:
    b _2

