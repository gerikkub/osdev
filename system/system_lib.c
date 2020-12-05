
#include <stdint.h>
#include <stdlib.h>

#include "system/system_lib.h"
#include "include/k_syscall.h"

void* system_map_device(uintptr_t device, uint64_t len) {
    intptr_t dev_ptr;
    SYSCALL_CALL_RET(SYSCALL_MAPDEV, device, len, 0, 0, dev_ptr);
    if (dev_ptr < 0) {
        return NULL;
    } else {
        return (void*)dev_ptr;
    }
}