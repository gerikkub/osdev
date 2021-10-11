
#ifndef __DRIVERS_H__
#define __DRIVERS_H__

#include <stdint.h>
#include <stdbool.h>

typedef void (*initfunc)(void);

#define REGISTER_DRIVER(x) \
    const initfunc __attribute__((section (".driver_init"))) x ## register_ptr = x ## _register;

void drivers_init(void);

typedef void (*initctxfunc)(void*);

enum {
    DRIVER_DISCOVERY_DTB = 1,
    DRIVER_DISCOVERY_PCI = 2
};

typedef struct {
    char* compat;
} discovery_dtb_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
} discovery_pci_t;

typedef struct {
    uint64_t type;

    union {
        discovery_dtb_t dtb;
        discovery_pci_t pci;
    };

    initctxfunc ctxfunc;
} discovery_register_t;

void register_driver(discovery_register_t* discovery);

bool discovery_have_driver(discovery_register_t* discovery);
void discover_driver(discovery_register_t* discovery, void* ctx);

#endif
