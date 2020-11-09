
#include <stdint.h>

#include "system/system_lib.h"

#include "kernel/syscall.h"

static volatile int dummy = 5;

void main(void* parameters) {

    char* my_str = "Hello Userspace!\n";

    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);

    (void)dummy;

    while(1);
}