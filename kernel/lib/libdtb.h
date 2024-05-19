
#ifndef __LIBDTB_H__
#define __LIBDTB_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/lib/llist.h"
#include "kernel/lib/intmap.h"

#define MAX_DTB_COMPAT_REG_NAME 256

/**
 * Device Tree Standard Properties
 */

enum {
    DT_PROP_COMPAT = 1,
    DT_PROP_MODEL = 2,
    DT_PROP_PHANDLE = 4,
    DT_PROP_REG = 8,
    DT_PROP_RANGES = 16,
    DT_PROP_DMA_RANGES = 32,
};

typedef struct {
    char* str;
    uint64_t len;
} dt_prop_stringlist_t;

typedef struct {
    char* str;
    uint64_t len;
} dt_prop_string_t;

typedef struct {
    uint32_t val;
} dt_prop_u32_t;

typedef struct {
    uint32_t* addr_ptr;
    uint64_t addr_size;
    uint32_t* size_ptr;
    uint64_t size_size;
} dt_prop_reg_entry_t;

typedef struct {
    dt_prop_reg_entry_t* reg_entries;
    uint64_t num_regs;
} dt_prop_reg_t;

typedef struct {
    uint32_t* addr_ptr;
    uint64_t addr_size;
    uint32_t* paddr_ptr;
    uint64_t paddr_size;
    uint32_t* size_ptr;
    uint64_t size_size;
} dt_prop_range_entry_t;

typedef struct {
    dt_prop_range_entry_t* range_entries;
    uint64_t num_ranges;
} dt_prop_ranges_t;

typedef struct {
    uint64_t child_addr;
    uint32_t pci_hi_addr;
    uint64_t parent_addr;
    uint64_t size;
} dt_prop_dma_range_entry_t;

typedef struct {
    uint64_t dma_range_entries_off;
    uint64_t num_dma_ranges;
} dt_prop_dma_ranges_t;

typedef struct {
    char* name;
    uint32_t* data;
    uint64_t data_len;
} dt_prop_generic_t;

typedef struct {
    uint32_t* data;
    uint64_t data_len;
} dt_prop_int_entry_t;

typedef struct {
    struct dt_node_* handler;
    dt_prop_int_entry_t* int_entries;
    uint64_t num_ints;
} dt_prop_ints_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header_t;

typedef enum {
    FDT_TOKEN_BEGIN_NODE = 0x1,
    FDT_TOKEN_END_NODE = 0x2,
    FDT_TOKEN_PROP = 0x3,
    FDT_TOKEN_NOP = 0x4,
    FDT_TOKEN_END = 0x9
} fdt_token_t;

typedef struct __attribute__((packed)) {
    uint32_t token;
    uint32_t len;
    uint32_t nameoff;
} fdt_property_token_t;

typedef struct {
    uint8_t* dtb_data;
    fdt_header_t* header;
} fdt_ctx_t;

typedef enum {
    FDT_PROP_TYPE_UNKNOWN = 0,
    FDT_PROP_TYPE_EMPTY,
    FDT_PROP_TYPE_U8,
    FDT_PROP_TYPE_U16,
    FDT_PROP_TYPE_U32,
    FDT_PROP_TYPE_U64,
    FDT_PROP_TYPE_STR
} fdt_property_type_t;

typedef struct dt_node_ {
    char* name;
    uint64_t address;

    llist_head_t properties;
    dt_prop_u32_t* prop_addr_cells;
    dt_prop_u32_t* prop_size_cells;
    dt_prop_u32_t* prop_int_cells;
    dt_prop_stringlist_t* prop_compat;
    dt_prop_string_t* prop_model;
    dt_prop_u32_t* prop_phandle;
    dt_prop_reg_t* prop_reg;
    dt_prop_ranges_t* prop_ranges;

    dt_prop_u32_t* prop_int_parent;

    dt_prop_ints_t* prop_ints;

    llist_head_t children;

    struct dt_node_* parent;

    void* dtb_funcs;
    void* dtb_ctx;
} dt_node_t;

struct dt_node_;

typedef struct {
    struct dt_node_* head;
    hashmap_ctx_t* phandle_map;
} dt_ctx_t;


typedef struct {
    dt_ctx_t* dt_ctx;
    dt_node_t* dt_node;
} discovery_dtb_ctx_t;

uintptr_t dt_map_addr_to_phy(dt_node_t* node, uintptr_t addr, bool* valid);

void dt_print_node_padding(uint64_t padding);
void dt_print_node(dt_ctx_t* ctx, dt_node_t* node, uint64_t padding, bool children);


#endif