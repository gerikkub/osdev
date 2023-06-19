
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stdlib/bitutils.h"

#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/llist.h"

#include "kernel/drivers.h"

extern uint8_t _driver_init_start;
extern uint8_t _driver_init_end;

static discovery_register_t* s_driver_registers;
static uint64_t s_driver_registers_end;
static uint64_t s_driver_registers_size;

typedef struct {
    initctxfunc fn;
    void* ctx;
} driver_late_init_ctx_t;

static llist_head_t s_driver_late_init_list;

void drivers_init(void) {

    initfunc* driver_driver_init = (initfunc*)&_driver_init_start;
    uint64_t num_drivers = ((uint64_t)(&_driver_init_end - &_driver_init_start)) / sizeof(void*);

    s_driver_registers = vmalloc(num_drivers * 4 * sizeof(discovery_register_t));
    s_driver_registers_size = num_drivers * 4;
    s_driver_registers_end = 0;

    s_driver_late_init_list = llist_create();

    for (uint64_t idx = 0; idx < num_drivers; idx++) {
        ASSERT((uintptr_t)driver_driver_init[idx] != 0);
        driver_driver_init[idx]();
    }
}


void register_driver(discovery_register_t* discovery) {

    ASSERT(discovery != NULL);
    ASSERT(s_driver_registers_end < s_driver_registers_size);

    s_driver_registers[s_driver_registers_end] = *discovery;
    s_driver_registers_end++;
}

static int64_t get_driver_idx(discovery_register_t* discovery) {


    for (uint64_t idx = 0; idx < s_driver_registers_end; idx++) {
        discovery_register_t* candidate = &s_driver_registers[idx];

        bool found = false;

        if (candidate->type == discovery->type) {
            switch(candidate->type) {
                case DRIVER_DISCOVERY_DTB:
                    found = strncmp(candidate->dtb.compat, discovery->dtb.compat, MAX_DTB_COMPAT_REG_NAME) == 0;
                    break;
                case DRIVER_DISCOVERY_PCI:
                    found = candidate->pci.vendor_id == discovery->pci.vendor_id &&
                            candidate->pci.device_id == discovery->pci.device_id;
                    break;
                default:
                    ASSERT(0);
            }
        }

        if (found) {
            return idx;
        }
    }

    return -1;
}

bool discovery_have_driver(discovery_register_t* discovery) {

    ASSERT(discovery != NULL);

    return get_driver_idx(discovery) >= 0;
}

void discover_driver(discovery_register_t* discovery, void* ctx) {

    ASSERT(discovery != NULL);

    int64_t driver_idx = get_driver_idx(discovery);
    if (driver_idx >= 0) {
        s_driver_registers[driver_idx].ctxfunc(ctx);
    }
}

void driver_register_late_init(initctxfunc fn, void* ctx) {
    driver_late_init_ctx_t* item = vmalloc(sizeof(driver_late_init_ctx_t));
    item->fn = fn;
    item->ctx = ctx;

    llist_append_ptr(s_driver_late_init_list, item);
} 

void driver_run_late_init(void) {
    driver_late_init_ctx_t* item = NULL;

    FOR_LLIST(s_driver_late_init_list, item)
        item->fn(item->ctx);
        vfree(item);
    END_FOR_LLIST()

    llist_free(s_driver_late_init_list);
}