
#ifndef __LIBPCI_H__
#define __LIBPCI_H__

#include <stdlib.h>
#include <stdbool.h>

#include "include/k_modules.h"

#define PCI_ADDR_HI_RELOCATABLE (1 << 31)
#define PCI_ADDR_HI_PREFETCHABLE (1 << 30)
#define PCI_ADDR_HI_ALIASED (1 << 29)

#define PCI_ADDR_HI_SPACE_MASK (3 << 24)
#define PCI_ADDR_HI_SPACE_CONFIG (0 << 24)
#define PCI_ADDR_HI_SPACE_IO (1 << 24)
#define PCI_ADDR_HI_SPACE_M32 (2 << 24)
#define PCI_ADDR_HI_SPACE_M64 (3 << 24)

#define PCI_ADDR_HI_BUS_MASK (0xFF << 16)
#define PCI_ADDR_HI_BUS_SHIFT (16)
#define PCI_ADDR_HI_DEVICE_MASK (0x1F << 11)
#define PCI_ADDR_HI_DEVICE_SHIFT (11)
#define PCI_ADDR_HI_FUNCTION_MASK (7 << 8)
#define PCI_ADDR_HI_FUNCTION_SHIFT (8)
#define PCI_ADDR_HI_REGISTER_MASK (0xFF << 0)
#define PCI_ADDR_HI_REGISTER_SHIFT (0)

#define MAX_PCI_BAR 6

typedef void (*pci_irq_handler_fn)(void* ctx);

typedef struct {
    uint64_t header_offset;

    uintptr_t header_phy;
    void* header_vmem;

    uintptr_t io_base_phy;
    void* io_base_vmem;
    uint64_t io_size;
    uintptr_t m32_base_phy;
    void* m32_base_vmem;
    uint64_t m32_size;
    uintptr_t m64_base_phy;
    void* m64_base_vmem;
    uint64_t  m64_size;

    struct {
        bool allocated;
        uint64_t space;
        uintptr_t phy;
        void* vmem;
        uintptr_t len;
    } bar[MAX_PCI_BAR];

    pci_irq_handler_fn int_fn;
    uint8_t int_num;
} pci_device_ctx_t;

typedef struct __attribute__((__packed__)) {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
} pci_header_t;

typedef struct __attribute__((__packed__)) {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar[6];
    uint32_t cis_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t rom_base_addr;
    uint8_t capabilities_ptr;
    uint8_t res0[3];
    uint32_t res1;
    uint8_t int_line;
    uint8_t int_pin;
    uint8_t min_grant;
    uint8_t max_latency;
} pci_header0_t;

#define PCI_BAR_REGION_MEM (0 << 0)
#define PCI_BAR_REGION_IO (1 << 0)
#define PCI_BAR_REGION_MASK (1 << 0)

#define PCI_BAR_IO_ADDR_MASK (0xFFFFFFFC)

#define PCI_BAR_MEM_LOC_M32 (0 << 1)
#define PCI_BAR_MEM_LOC_1M (1 << 1)
#define PCI_BAR_MEM_LOC_M64 (2 << 1)
#define PCI_BAR_MEM_LOC_MASK (3 << 1)

#define PCI_BAR_MEM_PREFETCHABLE (1 << 3)

#define PCI_BAR_MEM_ADDR_MASK (0xFFFFFFF0)

typedef enum {
    PCI_SPACE_CONFIG,
    PCI_SPACE_IO,
    PCI_SPACE_M32,
    PCI_SPACE_M64
} pci_space_t;

typedef struct {
    bool relocatable;
    bool prefetchable;
    bool aliased;
    pci_space_t space;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t reg;
    uint64_t addr;
} pci_addr_t;

typedef enum {
    PCI_CAP_NULL = 0,
    PCI_CAP_PMI = 1,
    PCI_CAP_AGP = 2,
    PCI_CAP_VPD = 3,
    PCI_CAP_SLOT_ID = 4,
    PCI_CAP_MSI = 5,
    PCI_CAP_COMPACTPPCI_HOT_SWAP = 6,
    PCI_CAP_PCIX = 7,
    PCI_CAP_HYPERTRANSPORT = 8,
    PCI_CAP_VENDOR = 9,
    PCI_CAP_DEBUG = 10,
    PCI_CAP_COMPACTPCI_CRC = 11,
    PCI_CAP_PCI_HOT_PLUG = 12,
    PCI_CAP_PCI_BSVID = 13,
    PCI_CAP_AGP_8X = 14,
    PCI_CAP_SECURE_DEVICE = 15,
    PCI_CAP_PCIE = 16,
    PCI_CAP_MSIX = 17,
    PCI_CAP_SERIAL_ATA = 18,
    PCI_CAP_ADVANCED_FEATURES = 19,
    PCI_CAP_ENHANCED_ALLOCATION = 20,
    PCI_CAP_FPBA = 21
} pci_capability_name_t;

typedef enum {
    PCI_VENDOR_QEMU = 0x1AF4
} pci_vendor_id_t;

typedef struct __attribute__((__packed__)) {
    uint8_t cap;
    uint8_t next;
    uint8_t payload[];
} pci_generic_capability_t;

typedef struct __attribute__((__packed__)) {
    uint8_t cap;
    uint8_t next;
    uint16_t msg_ctrl;
    uint32_t table_offset;
    uint32_t pba_offset;
} pci_msix_capability_t;

typedef struct __attribute__((__packed__)) {
    uint32_t msg_addr;
    uint32_t msg_uppr_addr;
    uint32_t msg_data;
    uint32_t vector_ctrl;
} pci_msix_table_entry_t;

typedef struct __attribute__((__packed__)) {
    uint8_t cap;
    uint8_t next;
    uint8_t len;
} pci_vendor_capability_t;

typedef struct {
    uintptr_t header_phy;
    uint64_t header_offset;

    uintptr_t io_base;
    uint64_t io_size;
    uintptr_t m32_base;
    uint64_t m32_size;
    uintptr_t m64_base;
    uint64_t  m64_size;

    struct {
        bool allocated;
        uint64_t space;
        uintptr_t phy;
        uint64_t len;
    } bar[6];
} discovery_pci_ctx_t;

typedef void (*pcie_irq_handler)(void* ctx);

typedef struct {
    pcie_irq_handler fn;
    void* ctx;
} pcie_int_handler_ctx_t;

void pci_alloc_device_from_context(pci_device_ctx_t* device, discovery_pci_ctx_t* module_ctx);

pci_generic_capability_t* pci_get_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t idx);

void print_pci_header(pci_device_ctx_t* device_ctx);
void print_pci_capabilities(pci_device_ctx_t* device_ctx);
void print_pci_capability_msix(pci_device_ctx_t* device_ctx, pci_msix_capability_t* cap_ptr);
void print_pci_capability_vendor(pci_device_ctx_t* device_ctx, pci_vendor_capability_t* cap_ptr);



void pci_register_int_handler(pci_device_ctx_t* device_ctx, pci_irq_handler_fn fn, void* fn_ctx);
void pci_wait_irq(pci_device_ctx_t* device_ctx);

#endif