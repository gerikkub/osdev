
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/syscall.h"
#include "kernel/exception.h"
#include "kernel/task.h"
#include "kernel/schedule.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vmem.h"
#include "kernel/elf.h"
#include "kernel/kernelspace.h"
#include "kernel/messages.h"
#include "kernel/modules.h"
#include "kernel/kmalloc.h"
#include "kernel/exec.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/net/net_api.h"
#include "kernel/select.h"
#include "kernel/taskctrl.h"

typedef int64_t (*syscall_handler)(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3);

static syscall_handler s_syscall_table[MAX_SYSCALL_NUM] = {0};

const char* syscall_print_table[] = {
    "Yield", "SendMsg",
    "GetMsgs", "StartMod", "MapDev",
    "Sbrk", "Open", "Read", "Write",
    "Ioctl", "Close"
};

static int64_t syscall_yield(uint64_t x0,
                              uint64_t x1,
                              uint64_t x2,
                              uint64_t x3) {

    // console_write("yield\n");
    return 0;
}

static int64_t syscall_mapdev(uint64_t phy_addr,
                              uint64_t len,
                              uint64_t flags,
                              uint64_t ctx) {
    memory_entry_device_t device_entry;

    task_t* task = get_active_task();

    memory_space_t* memspace = &task->memory;

    uint64_t ctx_phy;
    bool walk_ok;
    walk_ok = vmem_walk_table(task->low_vm_table, ctx, &ctx_phy);
    if (!walk_ok) {
        return SYSCALL_ERROR_BADARG;
    }
    syscall_mapdev_ctx_t* return_ctx = (syscall_mapdev_ctx_t*)(PHY_TO_KSPACE(ctx_phy));
    return_ctx->virt_addr = 0;
    return_ctx->phy_addr = 0;

    bool valid;
    valid = memspace_alloc_space(memspace, len, (memory_entry_t*)&device_entry);
    if (!valid) {
        return SYSCALL_ERROR_NOSPACE;
    }

    if (flags & SYSCALL_MAPDEV_ANYPHY) {
        device_entry.type = MEMSPACE_DEVICE;
        device_entry.phy_addr = (uintptr_t)kmalloc_phy(len);
    } else {
        //TODO: This lets userspace map any memory range into their
        //      memory. Potential security issue...
        device_entry.type = MEMSPACE_DEVICE;
        device_entry.phy_addr = phy_addr;
    }
    device_entry.flags = MEMSPACE_FLAG_PERM_URW;

    valid = memspace_add_entry_to_memory(memspace, (memory_entry_t*)&device_entry);
    if (!valid) {
        return SYSCALL_ERROR_NOSPACE;
    }

    // Rebuild vmem for task
    vmem_deallocate_table(task->low_vm_table);
    task->low_vm_table = memspace_build_vmem(memspace);

    return_ctx->virt_addr = device_entry.start;
    return_ctx->phy_addr = device_entry.phy_addr;
    return SYSCALL_ERROR_OK;
}

/**
 * sbrk(int64_t amount)
 * return: heap_limit_ptr
 * 
 * Increase process heap size by (amount). (amount) must
 * be positive.
 * The heap limit pointer is returned. Using an (amount) of 0
 * will return the current heap limit pointer
 */
static int64_t syscall_sbrk(uint64_t amount_unsigned,
                            uint64_t x1,
                            uint64_t x2,
                            uint64_t x3) {

    int64_t amount = (int64_t)amount_unsigned;
    
    if (amount < 0) {
        return SYSCALL_ERROR_BADARG;
    }

    task_t* this_task = get_active_task();
    memory_space_t* memspace = &this_task->memory;
    memory_entry_phy_t* entry;

    int64_t new_end = 0;

    // Look for a heap entry
    entry = (memory_entry_phy_t*)memspace_get_entry_at_addr(memspace, (void*)USER_ADDRSPACE_HEAP);
    if (entry == NULL) {
        if (amount == 0) {
            // No need to create a heap just yet
            return USER_ADDRSPACE_HEAP;
        } else {

            int64_t amount_pages = PAGE_CEIL(amount);
            void* heap_mem = kmalloc_phy(amount_pages);
            if (heap_mem == NULL) {
                return SYSCALL_ERROR_NOSPACE;
            }

            memory_entry_phy_t heap_entry = {
                .start = USER_ADDRSPACE_HEAP,
                .end = USER_ADDRSPACE_HEAP + amount_pages,
                .type = MEMSPACE_PHY,
                .flags = MEMSPACE_FLAG_PERM_URW,
                .phy_addr = (uintptr_t)heap_mem,
                .kmem_addr = PHY_TO_KSPACE(heap_mem)
            };

            bool add_ok;
            add_ok = memspace_add_entry_to_memory(memspace, (memory_entry_t*)&heap_entry);
            ASSERT(add_ok);

            new_end = heap_entry.end;
        }
    } else {
        ASSERT(entry->type == MEMSPACE_PHY);

        if (amount == 0) {
            return entry->end;
        } else {
            memspace_remove_entry_from_memory(memspace, (memory_entry_t*)entry);

            uint64_t old_amount = entry->end - entry->start;
            uint64_t new_amount = old_amount + PAGE_CEIL(amount);

            // Basically realloc. Eventually we could just add pages
            void* old_phy = (void*)entry->phy_addr;
            void* new_phy = kmalloc_phy(new_amount);
            if (new_phy == NULL) {
                return SYSCALL_ERROR_NOSPACE;
            }

            memcpy((void*)PHY_TO_KSPACE(new_phy), (void*)PHY_TO_KSPACE(old_phy), old_amount);

            kfree_phy(old_phy);

            // new_phy has been realloc'd. Now update entry with
            // the new size and address
            memory_entry_phy_t new_entry = {
                .start = entry->start,
                .end = entry->start + new_amount,
                .type = MEMSPACE_PHY,
                .flags = MEMSPACE_FLAG_PERM_URW,
                .phy_addr = (uintptr_t)new_phy,
                .kmem_addr = PHY_TO_KSPACE(new_phy)
            };

            memspace_add_entry_to_memory(memspace, (memory_entry_t*)&new_entry);

            new_end = new_entry.end;
        }
    }

    // Update the task's vm table
    this_task->low_vm_table = memspace_build_vmem(memspace);
    vmem_flush_tlb();

    // Return the new heap limit
    return new_end;
}


void syscall_sync_handler(uint64_t vector, uint32_t esr) {


    uint64_t syscall_num = esr & 0xFFFF;

    // TODO: Maybe don't assert but end a task
    // on this error
    ASSERT(syscall_num < MAX_SYSCALL_NUM);

    syscall_handler handler = s_syscall_table[syscall_num];

    // TODO: Maybe don't assert but end a task
    // on a nonexistent syscall
    ASSERT(handler != NULL);

    task_t* task_ptr = get_active_task();
    uint64_t ret;

    /*
    console_log(LOG_DEBUG, "Syscall %s Tid %u\n", syscall_print_table[syscall_num], task_ptr->tid);
    console_log(LOG_DEBUG, " ELR: %16x\n", task_ptr->reg.elr);
    console_log(LOG_DEBUG, " x0: %16x\n", task_ptr->reg.gp[0]);
    console_log(LOG_DEBUG, " x1: %16x\n", task_ptr->reg.gp[1]);
    console_log(LOG_DEBUG, " x2: %16x\n", task_ptr->reg.gp[2]);
    console_log(LOG_DEBUG, " x3: %16x\n", task_ptr->reg.gp[3]);
    */

    ret = handler(task_ptr->reg.gp[0],
                  task_ptr->reg.gp[1],
                  task_ptr->reg.gp[2],
                  task_ptr->reg.gp[3]);
                
    // Return the result in X0
    task_ptr->reg.gp[0] = ret;

    // Reschedule
    schedule();
}

int64_t syscall_exit(uint64_t ret_val, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    task_t* task_ptr = get_active_task();

    task_cleanup(task_ptr, ret_val);
    return (int64_t)ret_val;
}

int64_t syscall_exec(uint64_t device_arg, uint64_t path_arg, uint64_t task_name_arg, uint64_t argv_arg) {

    task_t* task = get_active_task();

    void* device_kptr = get_userspace_ptr(task->low_vm_table, device_arg);
    void* path_kptr = get_userspace_ptr(task->low_vm_table, path_arg);
    void* task_name_kptr = get_userspace_ptr(task->low_vm_table, task_name_arg);
    void* argv_kptr = get_userspace_ptr(task->low_vm_table, argv_arg);

    if (device_kptr == NULL ||
        path_kptr == NULL ||
        task_name_kptr == NULL) {
            
        return -1;
    }

    char** exec_argv = NULL;

    if (argv_kptr != NULL) {
        char** argv = argv_kptr;
        uint64_t exec_argc = 0;

        while (*argv != NULL) {
            exec_argc++;
            argv++;
        }

        exec_argv = vmalloc((exec_argc + 1) * sizeof(char*));

        argv = argv_kptr;
        uint64_t idx;
        for (idx = 0; idx < exec_argc; idx++) {
            argv_kptr = get_userspace_ptr(task->low_vm_table, (uintptr_t)argv[idx]);
            uint64_t arglen = strnlen(argv_kptr, EXEC_ARGV_ARG_MAXLEN);
            char* exec_arg = vmalloc(arglen);
            strncpy(exec_arg, argv[idx], EXEC_ARGV_ARG_MAXLEN);
            exec_argv[idx] = exec_arg;
        }

        exec_argv[idx] = NULL;
    }

    uint64_t exec_res = exec_user_task(device_kptr, path_kptr, task_name_kptr, exec_argv);

    if (exec_argv != NULL) {
        uint64_t idx = 0;
        while (exec_argv[idx] != NULL) {
            vfree(exec_argv[idx]);
            idx++;
        }

        vfree(exec_argv);
    }

    return exec_res != 0 ? (int64_t)exec_res : -1;
}

void syscall_init(void) {

    s_syscall_table[SYSCALL_YIELD] = syscall_yield;
    s_syscall_table[SYSCALL_GETMSGS] = syscall_getmsgs;
    s_syscall_table[SYSCALL_SENDMSG] = syscall_sendmsg;
    s_syscall_table[SYSCALL_STARTMOD] = syscall_startmod;
    s_syscall_table[SYSCALL_MAPDEV] = syscall_mapdev;
    s_syscall_table[SYSCALL_SBRK] = syscall_sbrk;
    s_syscall_table[SYSCALL_OPEN] = syscall_open;
    s_syscall_table[SYSCALL_READ] = syscall_read;
    s_syscall_table[SYSCALL_WRITE] = syscall_write;
    s_syscall_table[SYSCALL_IOCTL] = syscall_ioctl;
    s_syscall_table[SYSCALL_CLOSE] = syscall_close;
    s_syscall_table[SYSCALL_EXIT] = syscall_exit;
    s_syscall_table[SYSCALL_EXEC] = syscall_exec;
    s_syscall_table[SYSCALL_SOCKET] = syscall_socket;
    s_syscall_table[SYSCALL_BIND] = syscall_bind;
    s_syscall_table[SYSCALL_SELECT] = syscall_select;
    s_syscall_table[SYSCALL_TASKCTRL] = syscall_taskctrl;

    set_sync_handler(EC_SVC, syscall_sync_handler);
}
