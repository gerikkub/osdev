
#ifndef __LIBDTB_H__
#define __LIBDTB_H__

#include <stdint.h>

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

typedef enum {
    FDT_PROP_TYPE_UNKNOWN = 0,
    FDT_PROP_TYPE_EMPTY,
    FDT_PROP_TYPE_U8,
    FDT_PROP_TYPE_U16,
    FDT_PROP_TYPE_U32,
    FDT_PROP_TYPE_U64,
    FDT_PROP_TYPE_STR
} fdt_property_type_t;

typedef struct {
    uint8_t* data_ptr;
    uint64_t data_len;

    uint32_t nameoff;
} fdt_property_t;

typedef struct {
    char* name;
    bool name_valid;
    uint64_t* properties_list;
    int64_t num_properties;
    uint64_t* children_list;
    int64_t num_children;
} fdt_node_t;

typedef struct {
    uint8_t* fdt;
    fdt_header_t* header;
    fdt_property_t* property_list;
    uint64_t num_properties;
    fdt_node_t* node_list;
    uint64_t num_nodes;
} fdt_ctx_t;

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
    uint64_t compat_off;
} dt_prop_compat_t;

typedef struct {
    uint64_t model_off;
} dt_prop_model_t;

typedef struct {
    uint32_t phandle;
} dt_prop_phandle_t;

typedef struct {
    uint32_t addr;
    uint32_t size;
} dt_prop_cells_t;

typedef struct {
    uint64_t addr;
    uint64_t size;
} dt_prop_reg_entry_t;

typedef struct {
    uint64_t reg_entries_off;
    uint64_t num_regs;
} dt_prop_reg_t;

typedef struct {
    uint64_t child_addr;
    uint32_t pci_hi_addr;
    uint64_t parent_addr;
    uint64_t size;
} dt_prop_range_entry_t;

typedef struct {
    uint64_t range_entries_off;
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
    uint64_t data_off;
    uint64_t data_len;
    uint64_t name_off;
} dt_prop_generic_t;

typedef struct {
    uint64_t name_off;
    uint64_t address;

    uint64_t property_list_off;
    uint64_t num_properties;

    uint64_t node_list_off;
    uint64_t num_nodes;

    dt_prop_cells_t cells;
    dt_prop_cells_t parent_cells;

    uint32_t std_properties_mask;
    dt_prop_compat_t compat;
    dt_prop_model_t model;
    dt_prop_phandle_t phandle;
    dt_prop_reg_t reg;
    dt_prop_ranges_t ranges;
    dt_prop_dma_ranges_t dma_ranges;
} dt_node_t;

typedef struct {
    uint64_t node_off;
    uint64_t len;
    uint8_t data[];
} dt_block_t;

typedef struct {
    dt_block_t* block;
} discovery_dtb_ctx_t;

#define MAX_DTB_COMPAT_REG_NAME 128


char* fdt_get_string(fdt_header_t* header, uint8_t* dtbmem, uint32_t stroff);

/**
 * dt_build_block
 * 
 * Build a single block of memory that contains all device tree information
 * for a single node and its children. This block can then be passed to
 * other modules through a message call
 */
dt_block_t* dt_build_block(fdt_node_t* base_fdt_node, fdt_ctx_t* fdt_ctx, dt_prop_cells_t* parent_cells);

#endif