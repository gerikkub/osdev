

.arch armv8-a

.text

.global _start

.extern

_start:
    // Registers and stack should already be setup by the kernel
    // We can also assume data and bss section are setup correctly

    // Initialize system libraries
    bl system_init

    // Main should not return
    bl main

    // If main returns the kernel has setup a trap for us
    ret