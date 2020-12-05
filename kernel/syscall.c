
#include <stdint.h>
#include <stdbool.h>

#include "kernel/syscall.h"
#include "kernel/exception.h"
#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vmem.h"
#include "kernel/kernelspace.h"
#include "kernel/messages.h"
#include "kernel/modules.h"

typedef int64_t (*syscall_handler)(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3);

static syscall_handler s_syscall_table[MAX_SYSCALL_NUM] = {0};

const char* syscall_print_table[] = {
    "Yield", "Print", "SendMsg",
    "GetMsgs", "GetMod", "MapDev"
};

static int64_t syscall_yield(uint64_t x0,
                              uint64_t x1,
                              uint64_t x2,
                              uint64_t x3) {

    console_write("yield\n");
    return 0;
}

static int64_t syscall_console_print(uint64_t msg_intptr,
                                      uint64_t x1,
                                      uint64_t x2,
                                      uint64_t x3) {

    task_t* task = get_active_task();

    bool walk_ok;
    uint64_t phy_msg_addr;
    walk_ok = vmem_walk_table(task->low_vm_table, msg_intptr, &phy_msg_addr);
    if (walk_ok) {

        uint8_t* msg_kmem = (uint8_t*)PHY_TO_KSPACE(phy_msg_addr);

        // THIS IS BAD FOR SOOOO MANY REASONS
        // DELETE THIS AFTER PROOF OF CONCEPT!!!
        console_write((char*)msg_kmem);
    }

    return 0;
}

static int64_t syscall_mapdev(uint64_t phy_addr,
                              uint64_t len,
                              uint64_t x2,
                              uint64_t x3) {
    memory_entry_device_t device_entry;

    task_t* task = get_active_task();

    memory_space_t* memspace = &task->memory;

    bool valid;
    valid = memspace_alloc_space(memspace, len, (memory_entry_t*)&device_entry);
    if (!valid) {
        return SYSCALL_ERROR_NOSPACE;
    }

    device_entry.type = MEMSPACE_DEVICE;
    device_entry.flags = MEMSPACE_FLAG_PERM_URW;
    //TODO: This lets userspace map any memory range into their
    //      memory. Potential security issue...
    device_entry.phy_addr = phy_addr;

    valid = memspace_add_entry_to_memory(memspace, (memory_entry_t*)&device_entry);
    if (!valid) {
        return SYSCALL_ERROR_NOSPACE;
    }

    // Rebuild vmem for task
    vmem_deallocate_table(task->low_vm_table);
    task->low_vm_table = memspace_build_vmem(memspace);

    uint64_t start_addr = device_entry.start;
    return start_addr;
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

    console_log(LOG_DEBUG, "Syscall %s Tid %u\n", syscall_print_table[syscall_num], task_ptr->tid);
    console_log(LOG_DEBUG, " ELR: %16h\n", task_ptr->reg.elr);
    console_log(LOG_DEBUG, " x0: %16h\n", task_ptr->reg.gp[0]);
    console_log(LOG_DEBUG, " x1: %16h\n", task_ptr->reg.gp[1]);
    console_log(LOG_DEBUG, " x2: %16h\n", task_ptr->reg.gp[2]);
    console_log(LOG_DEBUG, " x3: %16h\n", task_ptr->reg.gp[3]);

    ret = handler(task_ptr->reg.gp[0],
                  task_ptr->reg.gp[1],
                  task_ptr->reg.gp[2],
                  task_ptr->reg.gp[3]);
                
    // Return the result in X0
    task_ptr->reg.gp[0] = ret;

    // Reschedule
    schedule();
}

void syscall_init(void) {

    s_syscall_table[SYSCALL_YIELD] = syscall_yield;
    s_syscall_table[SYSCALL_PRINT] = syscall_console_print;
    s_syscall_table[SYSCALL_GETMSGS] = syscall_getmsgs;
    s_syscall_table[SYSCALL_SENDMSG] = syscall_sendmsg;
    s_syscall_table[SYSCALL_GETMOD] = syscall_getmod;
    s_syscall_table[SYSCALL_MAPDEV] = syscall_mapdev;

    set_sync_handler(EC_SVC, syscall_sync_handler);
}