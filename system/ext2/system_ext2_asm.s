

.arch armv8-a


.text


.global state_test_asm

state_test_asm:
    mov x0, #1
    mov x1, #2
    mov x2, #3
    mov x3, #4
    mov x4, #5
    mov x5, #6
    mov x6, #7
    mov x7, #8
    mov x8, #9
    mov x9, #10
    mov x10, #11
    mov x11, #12
    mov x12, #13
    mov x13, #14
    mov x14, #15
    mov x15, #16
    mov x16, #17
    mov x17, #18
    mov x18, #19
    mov x19, #20
    mov x20, #21
    mov x21, #22
    mov x22, #23
    mov x23, #24
    mov x24, #25
    mov x25, #26
    mov x26, #27
    mov x27, #28
    mov x28, #29
    mov x29, #30
    mov x30, #31
    cmp x20, x21
    svc #0

_loop:
    b _loop
