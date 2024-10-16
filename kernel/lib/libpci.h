
#ifndef __LIBPCI_H__
#define __LIBPCI_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/lib/llist.h"

#include "stdlib/bitalloc.h"
#include "stdlib/linalloc.h"

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

#define PCI_GEN_ADDR(bus, dev, func, off) \
    ((((bus) & 0xFF) << 20) | \
     (((dev) & 0x1F) << 15) | \
     (((func) & 0x7) << 12) | \
     ((off) & 0xFFF))
#define PCI_DEV_ADDR(pci_dev, off) \
    PCI_GEN_ADDR(pci_dev->bus, pci_dev->dev, pci_dev->func, off)

#define PCI_GET_BUS(addr) (((addr) >> 20) & 0xFF)
#define PCI_GET_DEV(addr) (((addr) >> 16) & 0x1F)
#define PCI_GET_FUNC(addr) (((addr) >> 12) & 0x7)
#define PCI_GET_OFF(addr) ((addr) & 0xFFF)

struct pci_msix_capability_t;
struct pci_ctx_;
struct pci_device_ctx;

typedef void (*pci_irq_handler_fn)(uint32_t intid, void* ctx);

typedef void (*pci_cntr_read_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, void* header_ptr);
typedef uint8_t (*pci_cntr_read8_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, uint64_t offset);
typedef uint16_t (*pci_cntr_read16_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, uint64_t offset);
typedef uint32_t (*pci_cntr_read32_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, uint64_t offset);
typedef void (*pci_cntr_write8_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, uint64_t offset, uint8_t data);
typedef void (*pci_cntr_write16_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, uint64_t offset, uint16_t data);
typedef void (*pci_cntr_write32_header_fn)(struct pci_ctx_* ctx, uint64_t header_offset, uint64_t offset, uint32_t data);

#define PCI_HREAD8(pci_ctx, header_offset, off) \
    (pci_ctx->header_ops.read8(pci_ctx, header_offset, off))

#define PCI_HREAD16(pci_ctx, header_offset, off) \
    (pci_ctx->header_ops.read16(pci_ctx, header_offset, off))

#define PCI_HREAD32(pci_ctx, header_offset, off) \
    (pci_ctx->header_ops.read32(pci_ctx, header_offset, off))

#define PCI_HWRITE8(pci_ctx, header_offset, off, data) \
    (pci_ctx->header_ops.write8(pci_ctx, header_offset, off, data))

#define PCI_HWRITE16(pci_ctx, header_offset, off, data) \
    (pci_ctx->header_ops.write16(pci_ctx, header_offset, off, data))

#define PCI_HWRITE32(pci_ctx, header_offset, off, data) \
    (pci_ctx->header_ops.write32(pci_ctx, header_offset, off, data))

#define PCI_HREAD8_DEV(dev_ctx, off) \
    PCI_HREAD8((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), off)

#define PCI_HREAD16_DEV(dev_ctx, off) \
    PCI_HREAD16((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), off)

#define PCI_HREAD32_DEV(dev_ctx, off) \
    PCI_HREAD32((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), off)

#define PCI_HWRITE8_DEV(dev_ctx, off, data) \
    PCI_HWRITE8((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), off, data)

#define PCI_HWRITE16_DEV(dev_ctx, off, data) \
    PCI_HWRITE16((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), off, data)

#define PCI_HWRITE32_DEV(dev_ctx, off, data) \
    PCI_HWRITE32((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), off, data)


#define PCI_READBAR(pci_ctx, header_offset, idx) \
    (pci_ctx->header_ops.read32(pci_ctx, header_offset, 0x10 + (idx) * sizeof(uint32_t)))

#define PCI_WRITEBAR(pci_ctx, header_offset, idx, data) \
    (pci_ctx->header_ops.write32(pci_ctx, header_offset, 0x10 + (idx) * sizeof(uint32_t), data))

#define PCI_READBAR_DEV(dev_ctx, idx) \
    PCI_READBAR((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), idx)

#define PCI_WRITEBAR_DEV(dev_ctx, idx, data) \
    PCI_READBAR((dev_ctx)->pci_ctx, PCI_DEV_ADDR(dev_ctx, 0), idx, data)

typedef struct __attribute__((__packed__, aligned(512))) {
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

typedef struct __attribute__((__packed__, aligned(512))) {
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

typedef struct __attribute__((__packed__, aligned(512))) {
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
    uint32_t bar[2];
    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    uint8_t secondary_latency_timer;
    uint8_t io_base;
    uint8_t io_limit;
    uint16_t secondary_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint16_t prefetch_base;
    uint16_t prefetch_limit;
    uint32_t prefetch_base_upper;
    uint32_t prefetch_limit_upper;
    uint16_t io_base_upper;
    uint16_t io_limit_upper;
    uint8_t capabilities_ptr;
    uint8_t res0[3];
    uint32_t rom_addr;
    uint8_t irq_line;
    uint8_t irq_pint;
    uint16_t bridge_ctrl;
} pci_header1_t;

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

typedef enum {
    PCI_CAP_CAP = 0,
    PCI_CAP_NEXT = 1
} pci_cap_generic_t;

typedef struct {
    uint32_t offset;
    uint8_t cap;
    void* ctx;
} pci_cap_t;

#define PCI_READCAP8_DEV(dev_ctx, cap, off) \
    PCI_HREAD8_DEV((dev_ctx), ((cap)->offset) + (off))

#define PCI_READCAP16_DEV(dev_ctx, cap, off) \
    PCI_HREAD16_DEV((dev_ctx), ((cap)->offset) + (off))

#define PCI_READCAP32_DEV(dev_ctx, cap, off) \
    PCI_HREAD32_DEV((dev_ctx), ((cap)->offset) + (off))

#define PCI_WRITECAP8_DEV(dev_ctx, cap, off, data) \
    PCI_HWRITE8_DEV((dev_ctx), ((cap)->offset) + (off), (data))

#define PCI_WRITECAP16_DEV(dev_ctx, cap, off, data) \
    PCI_HWRITE16_DEV((dev_ctx), ((cap)->offset) + (off), (data))

#define PCI_WRITECAP32_DEV(dev_ctx, cap, off, data) \
    PCI_HWRITE32_DEV((dev_ctx), ((cap)->offset) + (off), (data))


#define PCI_CAP_MSIX_MSG_CTRL (2)
#define PCI_CAP_MSIX_TABLE_OFF (4)
#define PCI_CAP_MSIX_PBA_OFF (8)

#define PCI_MSIX_CTRL_SIZE_MASK (BIT(11) - 1)
#define PCI_MSIX_CTRL_FUNCTION_MASK BIT(14)
#define PCI_MSIX_CTRL_ENABLE BIT(15)

typedef struct __attribute__((__packed__)) {
    uint32_t msg_addr_lower;
    uint32_t msg_addr_upper;
    uint32_t msg_data;
    uint32_t vector_ctrl;
} pci_msix_table_entry_t;


#define PCI_MSIX_VECTOR_MASKED BIT(0)

#define PCI_CAP_VENDOR_LEN (2)

/*
typedef struct __attribute__((__packed__)) {
    uint8_t cap;
    uint8_t next;
    uint8_t len;
} pci_vendor_capability_t;
*/

typedef struct {
    pci_msix_table_entry_t* entry;
    uint32_t entry_idx;
    uint32_t intid;
    pci_irq_handler_fn handler;
    void* ctx;
} pci_msix_vector_ctx_t;

typedef void (*pcie_irq_handler)(uint32_t intid, void* ctx);

typedef struct {
    pcie_irq_handler fn;
    void* ctx;
} pcie_int_handler_ctx_t;


typedef struct {
    pci_addr_t io_pci_addr;
    uintptr_t io_mem_addr;
    uint64_t io_size;

    pci_addr_t m32_pci_addr;
    uintptr_t m32_mem_addr;
    uint64_t m32_size;

    pci_addr_t m64_pci_addr;
    uintptr_t m64_mem_addr;
    uint64_t m64_size;
} pci_range_t;

typedef struct {
    pci_range_t* range_ctx;
    uint64_t io_top;
    uint64_t m32_top;
    uint64_t m64_top;
} pci_alloc_t;

typedef struct {
    uint64_t device[3];
    uint64_t int_num;
    uint64_t int_ctx[3];
} pci_interrupt_map_entry_t;

typedef struct {
    uint32_t intid;
    llist_t* handler_list;
} pci_interrupt_ctx_t;

typedef struct {
    uint32_t device_mask[3];
    uint32_t int_mask;
    pci_interrupt_map_entry_t* entries;
    uint64_t num_entries;
    pci_interrupt_ctx_t int_ctx[4];
} pci_interrupt_map_t;

typedef struct {
    pci_cntr_read_header_fn read;
    pci_cntr_read8_header_fn read8;
    pci_cntr_read16_header_fn read16;
    pci_cntr_read32_header_fn read32;
    pci_cntr_write8_header_fn write8;
    pci_cntr_write16_header_fn write16;
    pci_cntr_write32_header_fn write32;
} pci_cntr_ops_t;

typedef struct pci_bridge_tree_ {
    uint8_t bus, dev, func;
    uint8_t class, subclass, progif;
    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;

    uint64_t io_base;
    uint64_t io_limit;
    uint64_t m32_base;
    uint64_t m32_limit;
    uint64_t m64_base;
    uint64_t m64_limit;

    struct pci_device_ctx* this_dev;

    llist_head_t children;
    llist_head_t devices;
} pci_bridge_tree_t;

typedef struct pci_ctx_ {
    pci_range_t ranges;
    pci_alloc_t dtb_alloc;
    pci_interrupt_map_t int_map;

    void* cntr_ctx;
    pci_cntr_ops_t header_ops;

    pci_bridge_tree_t bridge_tree;
    linalloc_t bus_alloc;
} pci_ctx_t;

typedef struct {
    pci_ctx_t* pci_ctx;
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


typedef struct pci_device_ctx {
    pci_ctx_t* pci_ctx;
    uint8_t bus, dev, func;

    uint16_t vendor_id, device_id;

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

    llist_head_t cap_list;

    pci_cap_t* msix_cap;
    llist_head_t msix_vector_list;
    bitalloc_t msix_vector_alloc;

} pci_device_ctx_t;

pci_cap_t* pci_get_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t idx);

void print_pci_header(pci_device_ctx_t* device_ctx);
void print_pci_capabilities(pci_device_ctx_t* device_ctx);
void print_pci_capability_msix(pci_device_ctx_t* device_ctx, pci_cap_t* cap);
void print_pci_capability_vendor(pci_device_ctx_t* device_ctx, pci_cap_t* cap);


uint32_t pci_register_interrupt_handler(pci_device_ctx_t* device_ctx, pci_irq_handler_fn fn, void* fn_ctx);
void pci_enable_interrupts(pci_device_ctx_t* device_ctx);
void pci_disable_interrupts(pci_device_ctx_t* device_ctx);
void pci_enable_vector(pci_device_ctx_t* device_ctx, uint32_t intid);
void pci_disable_vector(pci_device_ctx_t* device_ctx, uint32_t intid);
void pci_interrupt_clear_pending(pci_device_ctx_t* device_ctx, uint32_t intid);
pci_msix_vector_ctx_t* pci_get_msix_entry(pci_device_ctx_t* device_ctx, uint32_t intid);

void pci_wait_irq(pci_device_ctx_t* device_ctx);

void pci_poke_bar_entry(pci_header0_t* header, uint32_t barnum, uint32_t barval);

uint8_t pci_read8_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t cap_off);
uint16_t pci_read16_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t cap_off);
uint32_t pci_read32_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t cap_off);
void pci_write8_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t cap_off, uint8_t val);
void pci_write16_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t cap_off, uint16_t val);
void pci_write32_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t cap_off, uint32_t val);

#endif
