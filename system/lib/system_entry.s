

.arch armv8-a

.text

.global _start

.extern

_start: 
    // Registers and stack should already be setup by the kernel
    // We can also assume data and bss section are setup correctly

    // Initialize system libraries
    stp x0, x1, [sp, #-16]!
    bl system_init
    ldp x0, x1, [sp], #16
    
    // Main should not return
    bl main

    stp x0, x1, [sp, #-16]!
    bl system_deinit
    ldp x0, x1, [sp], #16

    // If main returns the kernel has setup a trap for us
    // Call syscall exit with the main return value
    svc #11
    ret