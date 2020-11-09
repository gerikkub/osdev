
#include <stdint.h>
#include <stdbool.h>

#include "kernel/bitutils.h"

#include "kernel/elf.h"
#include "kernel/assert.h"

#define MODULES_MAX_NUM 64

#define DEFINE_MODULE(name) \
    extern uint8_t name ## _start; \
    extern uint8_t name ## _end; \
    extern uint8_t name ## _size;

#define GET_ABS_SYM_HELPER(x, sym) GET_ABS_SYM(x, sym)

#define ADD_MODULE(idx, name) \
    { \
        uint64_t tmp_start; \
        uint64_t tmp_size; \
 \
        GET_ABS_SYM_HELPER(tmp_start, name ## _start); \
        GET_ABS_SYM_HELPER(tmp_size, name ## _size); \
        elf_module_list[idx].valid = true; \
        elf_module_list[idx].module_start = (uint8_t*)tmp_start; \
        elf_module_list[idx].module_size = tmp_size; \
    }

typedef struct {
    bool valid;
    uint8_t* module_start;
    uint64_t module_size;
    uint64_t tid;
} modules_list_t;

static modules_list_t elf_module_list[MODULES_MAX_NUM];

DEFINE_MODULE(_binary_system_build_ext2_ext2_elf)

void modules_init_list(void) {

    for (int idx = 0; idx < MODULES_MAX_NUM; idx++) {
        elf_module_list[idx].valid = false;
        elf_module_list[idx].tid = 0;
    }
    ADD_MODULE(0, _binary_system_build_ext2_ext2_elf)
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