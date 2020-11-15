
#include <stdint.h>

#include "system/system_lib.h"

#include "kernel/syscall.h"

static volatile int dummy = 5;

void state_test_asm(void);

void main(void* parameters) {

    char* my_str = "Hello Userspace!\n";

    state_test_asm();

    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);

    (void)dummy;

    while(1);
}
