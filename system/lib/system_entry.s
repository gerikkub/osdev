

.arch armv8-a

.text

.global _start

_start:
    // Registers and stack should already be setup by the kernel
    // We can also assume data and bss section are setup correctly

    // Main should not return
    bl main

    // If main returns the kernel has setup a trap for us
    ret