
#include <stdint.h>

#include "system/system_lib.h"

#include "kernel/syscall.h"


void main(void* parameters) {

    char* my_str = "VFS\n";

    while(1) {
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);
    }
}
