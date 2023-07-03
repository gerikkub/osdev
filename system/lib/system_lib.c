
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_console.h"
#include "system/lib/system_malloc.h"
#include "include/k_syscall.h"

void system_init(void) {
    malloc_init();
    console_open();
}

void system_deinit(void) {
    console_flush();
}

void* system_map_device(uintptr_t device, uint64_t len) {
    intptr_t dev_ptr;
    syscall_mapdev_ctx_t return_ctx = {0};
    SYSCALL_CALL_RET(SYSCALL_MAPDEV, device, len, 0, (uintptr_t)&return_ctx, dev_ptr);
    if (dev_ptr != SYSCALL_ERROR_OK) {
        return NULL;
    } else {
        return (void*)return_ctx.virt_addr;
    }
}

bool system_map_anyphy(uintptr_t len, uintptr_t* phy_out, uintptr_t* virt_out) {
    intptr_t ret;
    syscall_mapdev_ctx_t return_ctx = {0};
    uint64_t flags = SYSCALL_MAPDEV_ANYPHY;
    SYSCALL_CALL_RET(SYSCALL_MAPDEV, 0, len, flags, (uintptr_t)&return_ctx, ret);
    if (ret != SYSCALL_ERROR_OK) {
        return false;
    } else {
        if (phy_out != NULL) {
            *phy_out = return_ctx.phy_addr;
        }
        if (virt_out != NULL) {
            *virt_out = return_ctx.virt_addr;
        }
        return true;
    }

}

void system_yield(void) {
    SYSCALL_CALL(SYSCALL_YIELD, 0, 0, 0, 0);
}

int64_t system_exec(const char* device, const char* path, const char* name, char** const argv) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_EXEC, (uintptr_t)device, (uintptr_t)path, (uintptr_t)name, (uintptr_t)argv, ret);
    return ret;
}

int64_t system_select(syscall_select_ctx_t* select_arr, uint64_t select_len, uint64_t timeout_us, uint64_t* ready_mask_out) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_SELECT, (uintptr_t)select_arr, select_len, timeout_us, (uintptr_t)ready_mask_out, ret);
    return ret;
}