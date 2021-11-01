

.arch armv8-a

.text

.global _start

.extern

_start:
    // Registers and stack should already be setup by the kernel
    // We can also assume data and bss section are setup correctly

    // Initialize system libraries
    mov x16, x0
    mov x17, x1
    bl system_init
    mov x0, x16
    mov x1, x17

    // Main should not return
    bl main

    // If main returns the kernel has setup a trap for us
    ret