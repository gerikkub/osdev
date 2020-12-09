
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/modules.h"

#include "stdlib/bitutils.h"

#include "kernel/elf.h"
#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/syscall.h"
#include "kernel/kernelspace.h"

#define MAX_MODULES_NUM 64

#define MOD_SUBCLASS_MAX 256

#define DEFINE_MODULE(name) \
    extern uint8_t name ## _start; \
    extern uint8_t name ## _end; \
    extern uint8_t name ## _size;

#define GET_ABS_SYM_HELPER(x, sym) GET_ABS_SYM(x, sym)

#define ADD_MODULE(idx, name, class_, subclass_idx_, starttype_, onboot_, compat_) \
    { \
        uint64_t tmp_start; \
        uint64_t tmp_size; \
 \
        GET_ABS_SYM_HELPER(tmp_start, name ## _start); \
        GET_ABS_SYM_HELPER(tmp_size, name ## _size); \
        s_module_list[idx].valid = true; \
        s_module_list[idx].module_start = (uint8_t*)tmp_start; \
        s_module_list[idx].module_size = tmp_size; \
        s_module_list[idx].class = class_; \
        s_module_list[idx].subclass_idx = subclass_idx_; \
        s_module_list[idx].starttype = starttype_; \
        s_module_list[idx].onboot = onboot_; \
        strncpy(s_module_list[idx].compat, compat_, MAX_MODULE_COMPAT_LEN); \
    }

typedef struct {
    bool valid;
    uint8_t* module_start;
    uint64_t module_size;
    uint64_t tid;
    module_class_t class;
    uint64_t subclass_idx;
    module_starttype_t starttype;
    bool onboot;
    char compat[MAX_MODULE_COMPAT_LEN];
} modules_list_t;

static modules_list_t s_module_list[MAX_MODULES_NUM] = {0};
static module_subclass_t s_module_subclass_table[MOD_SUBCLASS_MAX] = {0};

DEFINE_MODULE(_binary_system_build_ext2_ext2_elf)
DEFINE_MODULE(_binary_system_build_vfs_vfs_elf)
DEFINE_MODULE(_binary_system_build_dtb_dtb_elf)

void modules_init_list(void) {

    for (int idx = 0; idx < MAX_MODULES_NUM; idx++) {
        s_module_list[idx].valid = false;
        s_module_list[idx].tid = 0;
    }
    for (int idx = 0; idx < MOD_SUBCLASS_MAX; idx++) {
        s_module_subclass_table[idx].class = MOD_CLASS_INVALID;
    }

    s_module_subclass_table[0].class = MOD_CLASS_VFS;
    strncpy(s_module_subclass_table[0].subclass_name, "", MOD_SUBCLASS_STRLEN);

    s_module_subclass_table[1].class = MOD_CLASS_FS;
    strncpy(s_module_subclass_table[1].subclass_name, "ext2", MOD_SUBCLASS_STRLEN);

    s_module_subclass_table[2].class = MOD_CLASS_DISCOVERY;
    strncpy(s_module_subclass_table[2].subclass_name, "dtb", MOD_SUBCLASS_STRLEN);

    ADD_MODULE(0, _binary_system_build_vfs_vfs_elf, MOD_CLASS_VFS, 0, MOD_STARTTYPE_ONE, true, "")
    ADD_MODULE(1, _binary_system_build_ext2_ext2_elf, MOD_CLASS_FS, 1, MOD_STARTTYPE_ONE, false, "pci-host-ecam-generic")
    ADD_MODULE(2, _binary_system_build_dtb_dtb_elf, MOD_CLASS_DISCOVERY, 2, MOD_STARTTYPE_ONE, true, "")
}

static uint64_t start_module(modules_list_t* mod) {
    ASSERT(mod != NULL);

    uint64_t tid;
    elf_result_t res;

    module_subclass_t* subclass = &s_module_subclass_table[mod->subclass_idx];

    uint64_t name_idx = 0;
    char mod_name[MAX_TASK_NAME_LEN] = {0};
    switch (subclass->class) {
        case MOD_CLASS_VFS:
            strncpy(mod_name, "VFS-", MAX_TASK_NAME_LEN - name_idx);
            name_idx += strlen("VFS-");
            break;
        case MOD_CLASS_FS:
            strncpy(mod_name, "FS-", MAX_TASK_NAME_LEN - name_idx);
            name_idx += strlen("FS-");
            break;
        case MOD_CLASS_DISCOVERY:
            strncpy(mod_name, "DISCOVERY-", MAX_TASK_NAME_LEN - name_idx);
            name_idx += strlen("DISCOVERY-");
            break;
        default:
            ASSERT(0);
    }

    strncpy(&mod_name[name_idx], subclass->subclass_name, MAX_TASK_NAME_LEN - name_idx);

    tid = create_elf_task(mod->module_start, mod->module_size, &res, true, mod_name);
    ASSERT(res == ELF_VALID);

    if (mod->starttype == MOD_STARTTYPE_ONE) {
        mod->tid = tid;
    } else {
        mod->tid = 0;
    }

    return tid;
}

void modules_start(void) {
    for (int idx = 0; idx < MAX_MODULES_NUM; idx++) {
        if (s_module_list[idx].valid &&
            s_module_list[idx].onboot) {
            start_module(&s_module_list[idx]);
        }
    }
}

/*
int64_t syscall_getmod(uint64_t class,
                       uint64_t subclass0,
                       uint64_t subclass1,
                       uint64_t x3) {

    char subclass_str[MOD_SUBCLASS_STRLEN] = {0};
    memcpy(&subclass_str[0], &subclass0, sizeof(subclass0));
    memcpy(&subclass_str[8], &subclass1, sizeof(subclass1));

    uint64_t subclass_idx;
    for (subclass_idx = 0; subclass_idx < MOD_SUBCLASS_MAX; subclass_idx++) {
        module_subclass_t* subclass;
        subclass = &s_module_subclass_table[subclass_idx];
        if (subclass->class == class &&
            strncmp(subclass_str, subclass->subclass_name, MOD_SUBCLASS_STRLEN) == 0) {
            break;
        }
    }

    if (subclass_idx == MOD_SUBCLASS_MAX) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    uint64_t module_idx;

    for (module_idx = 0; module_idx < MAX_MODULES_NUM; module_idx++) {
        modules_list_t* mod = &s_module_list[module_idx];
        if (mod->valid &&
            mod->class == class &&
            mod->subclass_idx == subclass_idx) {

            break;
        }
    }
    if (module_idx == MAX_MODULES_NUM) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    return s_module_list[module_idx].tid;
}
*/

int64_t find_mod_class(module_subclass_t* class_subclass) {
    ASSERT(class_subclass != NULL);

    uint64_t subclass_idx;
    for (subclass_idx = 0; subclass_idx < MOD_SUBCLASS_MAX; subclass_idx++) {
        module_subclass_t* subclass;
        subclass = &s_module_subclass_table[subclass_idx];
        if (subclass->class == class_subclass->class &&
            strncmp(class_subclass->subclass_name, subclass->subclass_name, MOD_SUBCLASS_STRLEN) == 0) {
            break;
        }
    }

    if (subclass_idx == MOD_SUBCLASS_MAX) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    uint64_t module_idx;

    for (module_idx = 0; module_idx < MAX_MODULES_NUM; module_idx++) {
        modules_list_t* mod = &s_module_list[module_idx];
        if (mod->valid &&
            mod->class == class_subclass->class &&
            mod->subclass_idx == subclass_idx) {

            break;
        }
    }
    if (module_idx == MAX_MODULES_NUM) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    return module_idx;
}

int64_t find_mod_compat(char* compat) {

    uint64_t idx;

    for (idx = 0; idx < MAX_MODULES_NUM; idx++) {
        modules_list_t* mod = &s_module_list[idx];
        if (mod->valid &&
            (strncmp(mod->compat, compat, MAX_MODULE_COMPAT_LEN) == 0)) {
            
            break;
        }
    }

    if (idx == MAX_MODULES_NUM) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    return idx;
}

int64_t syscall_startmod(uint64_t startmod_struct,
                         uint64_t x1,
                         uint64_t x2,
                         uint64_t x3) {

    task_t* this_task = get_active_task();

    // Find the structure in kernel space
    uint64_t startmod_phy;
    bool walk_ok;
    walk_ok = vmem_walk_table(this_task->low_vm_table, startmod_struct, &startmod_phy);
    if (!walk_ok) {
        return SYSCALL_ERROR_BADARG;
    }

    module_startmod_t* startmod = (module_startmod_t*)(PHY_TO_KSPACE(startmod_phy));

    // Find the module index specificed by the structure
    int64_t mod_idx;
    switch(startmod->startsel) {
        case MOD_STARTSEL_CLASS:
            mod_idx = find_mod_class(&startmod->data.class_subclass);
            break;
        case MOD_STARTSEL_COMPAT:
            mod_idx = find_mod_compat(startmod->data.compat.compat_name);
            break;
        default:
            return SYSCALL_ERROR_BADARG;
    }

    if (mod_idx < 0) {
        return mod_idx;
    }

    ASSERT(mod_idx < MAX_MODULES_NUM);
    modules_list_t* mod = &s_module_list[mod_idx];

    // Depending on the module STARTTYEP we either want to
    // start a new instance or return an existing instance
    uint64_t tid;
    switch (mod->starttype) {
        case MOD_STARTTYPE_ONE:
            if (mod->tid != 0) {
                // An instance for this module already exists. Return the existing tid
                tid = mod->tid;
            } else {
                // Start the module then return the new tid
                tid = start_module(mod);
            }
            break;
        case MOD_STARTTYPE_MANY:
            tid = start_module(mod);
            break;
        default:
            return SYSCALL_ERROR_BADARG;
    }

    // Return the tid of the new or existing
    // module to userspace
    return tid;
}