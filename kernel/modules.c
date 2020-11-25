
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/modules.h"

#include "kernel/bitutils.h"
#include "kernel/elf.h"
#include "kernel/assert.h"
#include "kernel/syscall.h"

#define MODULES_MAX_NUM 64

#define MOD_SUBCLASS_STRLEN 16
#define MOD_SUBCLASS_MAX 256

#define DEFINE_MODULE(name) \
    extern uint8_t name ## _start; \
    extern uint8_t name ## _end; \
    extern uint8_t name ## _size;

#define GET_ABS_SYM_HELPER(x, sym) GET_ABS_SYM(x, sym)

#define ADD_MODULE(idx, name, class_, subclass_idx_) \
    { \
        uint64_t tmp_start; \
        uint64_t tmp_size; \
 \
        GET_ABS_SYM_HELPER(tmp_start, name ## _start); \
        GET_ABS_SYM_HELPER(tmp_size, name ## _size); \
        elf_module_list[idx].valid = true; \
        elf_module_list[idx].module_start = (uint8_t*)tmp_start; \
        elf_module_list[idx].module_size = tmp_size; \
        elf_module_list[idx].class = class_; \
        elf_module_list[idx].subclass_idx = subclass_idx_; \
    }

typedef struct {
    bool valid;
    uint8_t* module_start;
    uint64_t module_size;
    uint64_t tid;
    module_class_t class;
    uint64_t subclass_idx;
} modules_list_t;

typedef struct {
    module_class_t class;
    char subclass_name[MOD_SUBCLASS_STRLEN];
} module_subclass_t;

static modules_list_t elf_module_list[MODULES_MAX_NUM];
static module_subclass_t module_subclass_table[MOD_SUBCLASS_MAX];

DEFINE_MODULE(_binary_system_build_ext2_ext2_elf)
DEFINE_MODULE(_binary_system_build_vfs_vfs_elf)

void modules_init_list(void) {

    for (int idx = 0; idx < MODULES_MAX_NUM; idx++) {
        elf_module_list[idx].valid = false;
        elf_module_list[idx].tid = 0;
    }
    for (int idx = 0; idx < MOD_SUBCLASS_MAX; idx++) {
        module_subclass_table[idx].class = MOD_CLASS_INVALID;
    }

    module_subclass_table[0].class = MOD_CLASS_VFS;
    strncpy(module_subclass_table[0].subclass_name, "", MOD_SUBCLASS_STRLEN);

    module_subclass_table[1].class = MOD_CLASS_FS;
    strncpy(module_subclass_table[1].subclass_name, "ext2", MOD_SUBCLASS_STRLEN);

    ADD_MODULE(0, _binary_system_build_vfs_vfs_elf, MOD_CLASS_VFS, 0)
    ADD_MODULE(1, _binary_system_build_ext2_ext2_elf, MOD_CLASS_FS, 1)
}

static void start_module(modules_list_t* mod) {
    uint64_t tid;
    elf_result_t res;
    tid = create_elf_task(mod->module_start, mod->module_size, &res, true);
    ASSERT(res == ELF_VALID);
    mod->tid = tid;
}

void modules_start(void) {
    for (int idx = 0; idx < MODULES_MAX_NUM; idx++) {
        if (elf_module_list[idx].valid) {
            start_module(&elf_module_list[idx]);
        }
    }
}

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
        subclass = &module_subclass_table[subclass_idx];
        if (subclass->class == class &&
            strncmp(subclass_str, subclass->subclass_name, MOD_SUBCLASS_STRLEN) == 0) {
            break;
        }
    }

    if (subclass_idx == MOD_SUBCLASS_MAX) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    uint64_t module_idx;

    for (module_idx = 0; module_idx < MODULES_MAX_NUM; module_idx++) {
        modules_list_t* mod = &elf_module_list[module_idx];
        if (mod->valid &&
            mod->class == class &&
            mod->subclass_idx == subclass_idx) {

            break;
        }
    }
    if (module_idx == MODULES_MAX_NUM) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    return elf_module_list[module_idx].tid;
}