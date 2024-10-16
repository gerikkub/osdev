
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/drivers.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/intmap.h"
#include "kernel/lib/utils.h"

#include "stdlib/bitutils.h"

#define MAX_DTB_PROPERTIES 8192
#define MAX_DTB_NODES 1024
#define MAX_DTB_NODE_PROPERTIES 1024
#define MAX_DTB_PROPERTY_LEN 256
#define MAX_DTB_NODE_NAME 64

static dt_ctx_t* dt_ctx = NULL;

char* fdt_get_string(fdt_header_t* header, uint8_t* dtbmem, uint32_t stroff) {
    ASSERT(header != NULL);
    
    return (char*)&dtbmem[header->off_dt_strings + stroff];
}

void get_fdt_header(void* header_ptr, fdt_header_t* header_out) {
    const fdt_header_t* header = header_ptr;
    ASSERT(header != NULL);

    fdt_header_t header_fix;

    header_fix.magic = en_swap_32(header->magic);
    header_fix.totalsize = en_swap_32(header->totalsize);
    header_fix.off_dt_struct = en_swap_32(header->off_dt_struct);
    header_fix.off_dt_strings = en_swap_32(header->off_dt_strings);
    header_fix.off_mem_rsvmap = en_swap_32(header->off_mem_rsvmap);
    header_fix.version = en_swap_32(header->version);
    header_fix.last_comp_version = en_swap_32(header->last_comp_version);
    header_fix.boot_cpuid_phys = en_swap_32(header->boot_cpuid_phys);
    header_fix.size_dt_strings = en_swap_32(header->size_dt_strings);
    header_fix.size_dt_struct = en_swap_32(header->size_dt_struct);

    if (header_fix.magic != 0xD00DFEED) {
        return;
    }

    *header_out = header_fix;
}

bool is_token_nop(uint32_t token) {
    return en_swap_32(token) == FDT_TOKEN_NOP;
}

bool is_token_value_or_nop(uint32_t token, fdt_token_t token_val) {
    return is_token_nop(token) || (en_swap_32(token) == token_val);
}

static dt_prop_stringlist_t *fdt_create_stringlist_prop(fdt_ctx_t *fdt_ctx, uint8_t *dtb_ptr,
                                                        dt_ctx_t *dt_ctx, dt_node_t *node,
                                                        fdt_property_token_t* token, char *prop_name) {

    dt_prop_stringlist_t* prop = vmalloc(sizeof(dt_prop_stringlist_t));

    uint8_t* val_ptr = dtb_ptr + sizeof(fdt_property_token_t);
    uint32_t val_len = en_swap_32(token->len);
    prop->len = val_len;
    prop->str = vmalloc(val_len);
    memcpy(prop->str, val_ptr, val_len);

    return prop;
}

static dt_prop_string_t *fdt_create_string_prop(fdt_ctx_t *fdt_ctx, uint8_t *dtb_ptr,
                                                dt_ctx_t *dt_ctx, dt_node_t *node,
                                                fdt_property_token_t* token, char *prop_name) {

    dt_prop_string_t* prop = vmalloc(sizeof(dt_prop_string_t));

    uint8_t* val_ptr = dtb_ptr + sizeof(fdt_property_token_t);
    uint32_t val_len = en_swap_32(token->len);
    prop->len = val_len;
    prop->str = vmalloc(val_len);
    memcpy(prop->str, val_ptr, val_len);

    return prop;
}

static dt_prop_u32_t *fdt_create_u32_prop(fdt_ctx_t *fdt_ctx, uint8_t *dtb_ptr,
                                          dt_ctx_t *dt_ctx, dt_node_t *node,
                                          fdt_property_token_t* token, char *prop_name) {
    dt_prop_u32_t* prop = vmalloc(sizeof(dt_prop_u32_t));

    uint32_t* val_ptr = (uint32_t*)(dtb_ptr + sizeof(fdt_property_token_t));
    prop->val = en_swap_32(*val_ptr);

    return prop;
}

static dt_prop_reg_t* fdt_create_reg_prop(fdt_ctx_t* fdt_ctx, uint8_t* dtb_ptr,
                                          dt_ctx_t* dt_ctx, dt_node_t* node,
                                          fdt_property_token_t* token, char* prop_name) {
    dt_prop_reg_t* prop = vmalloc(sizeof(dt_prop_reg_t));

    ASSERT(node->parent);
    ASSERT(node->parent->prop_addr_cells != NULL);
    ASSERT(node->parent->prop_size_cells != NULL);

    uint32_t addr_cells = node->parent->prop_addr_cells->val;
    uint32_t size_cells = node->parent->prop_size_cells->val;
    uint32_t row_size = (addr_cells + size_cells) * sizeof(uint32_t);
    uint64_t num_rows = en_swap_32(token->len) / row_size;

    prop->reg_entries = vmalloc(num_rows * sizeof(dt_prop_reg_entry_t));
    prop->num_regs = num_rows;

    uint32_t* reg_ptr = (uint32_t*)(dtb_ptr + sizeof(fdt_property_token_t));
    for (uint64_t idx = 0; idx < num_rows; idx++) {

        prop->reg_entries[idx].addr_ptr = vmalloc(addr_cells * sizeof(uint32_t));
        prop->reg_entries[idx].addr_size = addr_cells;

        prop->reg_entries[idx].size_ptr = vmalloc(size_cells * sizeof(uint32_t));
        prop->reg_entries[idx].size_size = size_cells;

        for (uint64_t cell_idx = 0; cell_idx < addr_cells; cell_idx++)
        {
            prop->reg_entries[idx].addr_ptr[cell_idx] = en_swap_32(*reg_ptr);
            reg_ptr++;
        }

        for (uint64_t cell_idx = 0; cell_idx < size_cells; cell_idx++)
        {
            prop->reg_entries[idx].size_ptr[cell_idx] = en_swap_32(*reg_ptr);
            reg_ptr++;
        }
    }

    return prop;
}

static dt_prop_ranges_t* fdt_create_ranges_prop(fdt_ctx_t* fdt_ctx, uint8_t* dtb_ptr,
                                                dt_ctx_t* dt_ctx, dt_node_t* node,
                                                fdt_property_token_t* token, char* prop_name) {
    dt_prop_ranges_t* prop = vmalloc(sizeof(dt_prop_ranges_t));

    ASSERT(node->parent);
    ASSERT(node->parent->prop_addr_cells != NULL);
    ASSERT(node->prop_addr_cells != NULL);
    ASSERT(node->prop_size_cells != NULL);

    uint32_t paddr_cells = node->parent->prop_addr_cells->val;
    uint32_t addr_cells = node->prop_addr_cells->val;
    uint32_t size_cells = node->prop_size_cells->val;
    uint32_t row_size = (paddr_cells + addr_cells + size_cells) * sizeof(uint32_t);
    uint64_t num_rows = en_swap_32(token->len) / row_size;

    prop->range_entries = vmalloc(num_rows * sizeof(dt_prop_range_entry_t));
    prop->num_ranges = num_rows;

    uint32_t* reg_ptr = (uint32_t*)(dtb_ptr + sizeof(fdt_property_token_t));
    for (uint64_t idx = 0; idx < num_rows; idx++) {

        prop->range_entries[idx].addr_ptr = vmalloc(addr_cells * sizeof(uint32_t));
        prop->range_entries[idx].addr_size = addr_cells;

        prop->range_entries[idx].paddr_ptr = vmalloc(paddr_cells * sizeof(uint32_t));
        prop->range_entries[idx].paddr_size = paddr_cells;

        prop->range_entries[idx].size_ptr = vmalloc(size_cells * sizeof(uint32_t));
        prop->range_entries[idx].size_size = size_cells;

        for (uint64_t cell_idx = 0; cell_idx < addr_cells; cell_idx++) {
            prop->range_entries[idx].addr_ptr[cell_idx] = en_swap_32(*reg_ptr);
            reg_ptr++;
        }

        for (uint64_t cell_idx = 0; cell_idx < paddr_cells; cell_idx++) {
            prop->range_entries[idx].paddr_ptr[cell_idx] = en_swap_32(*reg_ptr);
            reg_ptr++;
        }

        for (uint64_t cell_idx = 0; cell_idx < size_cells; cell_idx++) {
            prop->range_entries[idx].size_ptr[cell_idx] = en_swap_32(*reg_ptr);
            reg_ptr++;
        }
    }

    return prop;
}

static dt_prop_generic_t* fdt_create_generic_prop(fdt_ctx_t* fdt_ctx, uint8_t* dtb_ptr,
                                                  dt_ctx_t* dt_ctx, dt_node_t* node,
                                                  fdt_property_token_t* token, char* prop_name) {
    dt_prop_generic_t* prop = vmalloc(sizeof(dt_prop_generic_t));

    uint64_t name_len = strnlen(prop_name, MAX_DTB_NODE_NAME);
    prop->name = vmalloc(name_len + 1);
    memset(prop->name, 0, name_len + 1);
    memcpy(prop->name, prop_name, name_len);

    uint64_t prop_len = en_swap_32(token->len);
    prop->data_len = prop_len;
    prop->data = vmalloc(prop_len);
    memcpy(prop->data, dtb_ptr + sizeof(fdt_property_token_t), prop_len);

    return prop;
}

void dt_register_phandle(dt_ctx_t* dt_ctx, dt_node_t* node) {

    ASSERT(node->prop_phandle != NULL);

    uint64_t* phandle_ptr = vmalloc(sizeof(uint64_t));
    *phandle_ptr = node->prop_phandle->val;
    if (hashmap_contains(dt_ctx->phandle_map, phandle_ptr)) {
        console_log(LOG_WARN, "Duplicate phandle values in DT (%d)", *phandle_ptr);
        vfree(phandle_ptr);
        return;
    }

    hashmap_add(dt_ctx->phandle_map, phandle_ptr, node);
}

uint8_t* add_fdt_cells_property(fdt_ctx_t* fdt_ctx, uint8_t* dtb_ptr, dt_ctx_t* dt_ctx, dt_node_t* node) {

    ASSERT(dtb_ptr != NULL);

    fdt_property_token_t* token = (fdt_property_token_t*)dtb_ptr;

    ASSERT(en_swap_32(token->token) == FDT_TOKEN_PROP);

    uint8_t* prop_value_ptr = dtb_ptr + sizeof(fdt_property_token_t);
    uint32_t prop_len = en_swap_32(token->len);

    char* prop_name = fdt_get_string(fdt_ctx->header, fdt_ctx->dtb_data, en_swap_32(token->nameoff));

    if (!strcmp(prop_name, "#address-cells")) {
        node->prop_addr_cells = fdt_create_u32_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else if (!strcmp(prop_name, "#size-cells")) {
        node->prop_size_cells = fdt_create_u32_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else if (!strcmp(prop_name, "#interrupt-cells")) {
        node->prop_int_cells = fdt_create_u32_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    }

    // Skip over padding added to the property
    // to align the next token to a uint32_t
    uintptr_t prop_padded = (uintptr_t)(prop_value_ptr + prop_len);
    prop_padded = (prop_padded + sizeof(uint32_t) - 1) & (~(sizeof(uint32_t) - 1));

    return (uint8_t*)prop_padded;
}

uint8_t* add_fdt_property(fdt_ctx_t* fdt_ctx, uint8_t* dtb_ptr, dt_ctx_t* dt_ctx, dt_node_t* node) {

    ASSERT(dtb_ptr != NULL);

    fdt_property_token_t* token = (fdt_property_token_t*)dtb_ptr;

    ASSERT(en_swap_32(token->token) == FDT_TOKEN_PROP);

    uint8_t* prop_value_ptr = dtb_ptr + sizeof(fdt_property_token_t);
    uint32_t prop_len = en_swap_32(token->len);

    char* prop_name = fdt_get_string(fdt_ctx->header, fdt_ctx->dtb_data, en_swap_32(token->nameoff));

    if (!strcmp(prop_name, "compatible")) {
        node->prop_compat = fdt_create_stringlist_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else if (!strcmp(prop_name, "model")) {
        node->prop_model = fdt_create_string_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else if (!strcmp(prop_name, "phandle")) {
        node->prop_phandle = fdt_create_u32_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
        dt_register_phandle(dt_ctx, node);
    } else if (!strcmp(prop_name, "#address-cells")) {
        // Handled previously. Skip
    } else if (!strcmp(prop_name, "#size-cells")) {
        // Handled previously. Skip
    } else if (!strcmp(prop_name, "reg")) {
        node->prop_reg = fdt_create_reg_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else if (!strcmp(prop_name, "ranges")) {
        node->prop_ranges = fdt_create_ranges_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else if (!strcmp(prop_name, "interrupt-parent")) {
        node->prop_int_parent = fdt_create_u32_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
    } else {
        dt_prop_generic_t* new_prop = fdt_create_generic_prop(fdt_ctx, dtb_ptr, dt_ctx, node, token, prop_name);
        llist_append_ptr(node->properties, new_prop);
    }

    // Skip over padding added to the property
    // to align the next token to a uint32_t
    uintptr_t prop_padded = (uintptr_t)(prop_value_ptr + prop_len);
    prop_padded = (prop_padded + sizeof(uint32_t) - 1) & (~(sizeof(uint32_t) - 1));

    return (uint8_t*)prop_padded;
}

dt_node_t* fdt_create_node(void) {
    dt_node_t* new_node = vmalloc(sizeof(dt_node_t));
    new_node->name = NULL;

    new_node->prop_addr_cells = NULL;
    new_node->prop_size_cells = NULL;
    new_node->prop_int_cells = NULL;
    new_node->prop_compat = NULL;
    new_node->prop_model = NULL;
    new_node->prop_phandle = NULL;
    new_node->prop_reg = NULL;
    new_node->prop_ranges = NULL;

    new_node->prop_int_parent = NULL;
    new_node->prop_ints = NULL;

    new_node->properties = llist_create();
    new_node->children = llist_create();

    new_node->dtb_funcs = NULL;
    new_node->dtb_ctx = NULL;

    return new_node;
}

uint8_t* get_fdt_node(fdt_ctx_t* fdt_ctx, uint8_t* dtb_ptr, dt_ctx_t* dt_ctx, dt_node_t* node, bool is_root) {

    
    ASSERT(dtb_ptr != NULL);
    ASSERT(node != NULL);

    uint32_t* token_ptr = (uint32_t*)dtb_ptr;
    uint32_t token = en_swap_32(*token_ptr);
    ASSERT(token == FDT_TOKEN_BEGIN_NODE);

    token_ptr++;

    // Read in the name as a null terminated string
    if (is_root) {
        node->name = vmalloc(2);
        memset(node->name, 0, 2);
        node->name[0] = '/';

        token_ptr++;
    } else {
        char* node_name_ptr = (char*)token_ptr;
        uint64_t name_len = strnlen(node_name_ptr, MAX_DTB_NODE_NAME) + 1;
        node->name = vmalloc(name_len);
        memset(node->name, 0, name_len);
        memcpy(node->name, node_name_ptr, name_len-1);

        for (int idx = 0; idx < name_len; idx++) {
            if (node->name[idx] == '@') {
                node->name[idx] = '\0';
                node->address = strtoull(&node->name[idx+1], NULL, 16);
            }

        }

        token_ptr = (uint32_t*)((uint8_t*)token_ptr + name_len);
    }

    // Optionally skip over padding added to the name
    // to align the next token to a uint32_t
    token_ptr = (uint32_t*)(((uintptr_t)token_ptr + 3) & ~(0x3));

    // Scan for cells
    uint32_t* cells_token_ptr = token_ptr;
    while (is_token_value_or_nop(*cells_token_ptr, FDT_TOKEN_PROP)) {
        if (is_token_nop(*cells_token_ptr)) {
            cells_token_ptr++;
        } else {
            cells_token_ptr = (uint32_t*)add_fdt_cells_property(fdt_ctx, (uint8_t*)cells_token_ptr, dt_ctx, node);
        }
    }

    // Create fake addr (2) and size (1) cells if they don't exist
    if (node->prop_addr_cells == NULL) {
        node->prop_addr_cells = vmalloc(sizeof(dt_prop_u32_t));
        node->prop_addr_cells->val = 2;
    }

    if (node->prop_size_cells == NULL) {
        node->prop_size_cells = vmalloc(sizeof(dt_prop_u32_t));
        node->prop_size_cells->val = 1;
    }

    // Inherit interrupt-properties from parent
    if (node->parent != NULL) {
        node->prop_int_parent = node->parent->prop_int_parent;
    }

    // Parse all properties first. These are guaranteed to be
    // before child nodes
    while (is_token_value_or_nop(*token_ptr, FDT_TOKEN_PROP)) {
        if (is_token_nop(*token_ptr)) {
            token_ptr++;
        } else {
            token_ptr = (uint32_t*)add_fdt_property(fdt_ctx, (uint8_t*)token_ptr, dt_ctx, node);
        }
    }

    // Parse all child nodes
    while (is_token_value_or_nop(*token_ptr, FDT_TOKEN_BEGIN_NODE)) {
        if (is_token_nop(*token_ptr)) {
            token_ptr++;
        } else {
            // Allocate a node
            dt_node_t* child_node = fdt_create_node();
            child_node->parent = node;

            // Recurse
            token_ptr = (uint32_t*)get_fdt_node(fdt_ctx, (uint8_t*)token_ptr, dt_ctx, child_node, false);

            llist_append_ptr(node->children, child_node);
        }
    }

    // Skip over remaining NOPs
    while (is_token_nop(*token_ptr)) {
        token_ptr++;
    }

    // Confirm that the final token is an END_NODE
    ASSERT(en_swap_32(*token_ptr) == FDT_TOKEN_END_NODE);
    token_ptr++;

    return (uint8_t*)token_ptr;
}

void fdt_print_header(fdt_header_t* fdt_header) {
    console_log(LOG_DEBUG, "FDT Header");
    console_log(LOG_DEBUG, " Magic: %8x", fdt_header->magic);
    console_log(LOG_DEBUG, " Total Size: %d", fdt_header->totalsize);
    console_log(LOG_DEBUG, " Struct Offset: %8x", fdt_header->off_dt_struct);
    console_log(LOG_DEBUG, " String Offset : %8x", fdt_header->off_dt_strings);
    console_log(LOG_DEBUG, " Mem Reserve Map Offset: %8x", fdt_header->off_mem_rsvmap);
    console_log(LOG_DEBUG, " Version: %d", fdt_header->version);
    console_log(LOG_DEBUG, " Last Comp Version: %8x", fdt_header->last_comp_version);
    console_log(LOG_DEBUG, " Boot CPUID: %8x", fdt_header->boot_cpuid_phys);
    console_log(LOG_DEBUG, " Strings Size: %8x", fdt_header->size_dt_strings);
    console_log(LOG_DEBUG, " Struct Size: %8x", fdt_header->size_dt_struct);
}

static dt_prop_ints_t* dtb_create_ints_prop(dt_ctx_t* ctx, dt_node_t* node, dt_prop_generic_t* prop) {

    dt_prop_ints_t* ints_prop = vmalloc(sizeof(dt_prop_ints_t));

    ASSERT(node->prop_int_parent != NULL);
    uint64_t phandle = node->prop_int_parent->val;
    dt_node_t* intc_node = hashmap_get(ctx->phandle_map, &phandle);
    ASSERT(intc_node);
    ASSERT(intc_node->prop_int_cells);

    uint64_t int_cells = intc_node->prop_int_cells->val;
    uint64_t num_entries = (prop->data_len/4) / int_cells;

    ints_prop->int_entries = vmalloc(sizeof(dt_prop_int_entry_t) * num_entries);
    ints_prop->num_ints = num_entries;
    ints_prop->handler = intc_node;

    for (uint64_t idx = 0; idx < num_entries; idx++) {
        ints_prop->int_entries[idx].data = vmalloc(int_cells * sizeof(uint32_t));

        for (uint64_t cell_idx = 0; cell_idx < int_cells; cell_idx++) {
            ints_prop->int_entries[idx].data[cell_idx] = en_swap_32(prop->data[idx*int_cells + cell_idx]);
        }
    }

    return ints_prop;
}

static void dtb_post_process(dt_ctx_t* ctx, dt_node_t* node) {

    dt_prop_generic_t* prop;
    FOR_LLIST(node->properties, prop)
        if (!strncmp(prop->name, "interrupts", MAX_DTB_PROPERTY_LEN)) {
            node->prop_ints = dtb_create_ints_prop(ctx, node, prop);
        }
    END_FOR_LLIST()

    dt_node_t* child_node;
    FOR_LLIST(node->children, child_node)
        dtb_post_process(ctx, child_node);
    END_FOR_LLIST()

}

static void dtb_discover(dt_ctx_t* ctx, dt_node_t* node) {

    if (node->prop_compat != NULL) {
        discovery_register_t disc_reg = {
            .type = DRIVER_DISCOVERY_DTB,
            .dtb = {
                .compat = (char *)node->prop_compat->str},
            .ctxfunc = NULL};


        if (discovery_have_driver(&disc_reg))
        {
            console_log(LOG_DEBUG, "Discovered %s", node->prop_compat->str);

            discovery_dtb_ctx_t disc_ctx = {
                .dt_ctx = ctx,
                .dt_node = node
            };

            discover_driver(&disc_reg, &disc_ctx);
        }
    }

    dt_node_t* child_node;
    FOR_LLIST(node->children, child_node)
        dtb_discover(ctx, child_node);
    END_FOR_LLIST()
}

void dtb_init(uintptr_t dtb_init_phy_addr) {

    void* header_phy_ptr = (void*)dtb_init_phy_addr;
    uint8_t* devicetree = (uint8_t*)PHY_TO_KSPACE(header_phy_ptr);

    memspace_map_phy_kernel(header_phy_ptr,
                            devicetree,
                            VMEM_PAGE_SIZE, MEMSPACE_FLAG_PERM_KRO);
    memspace_update_kernel_vmem();

    fdt_header_t header;
    get_fdt_header(devicetree, &header);

    // Remap the entire DTB

    memory_entry_t* old_entry = memspace_get_entry_at_addr_kernel(devicetree);
    memspace_unmap_kernel(old_entry);
    memspace_update_kernel_vmem();

    memspace_map_phy_kernel(header_phy_ptr,
                            devicetree,
                            header.totalsize, MEMSPACE_FLAG_PERM_KRO);
    memspace_update_kernel_vmem();


    fdt_ctx_t fdt_ctx = {
        .dtb_data = devicetree,
        .header = &header
    };

    uint8_t* dtb_tree = (devicetree + header.off_dt_struct);

    dt_node_t* root_node = fdt_create_node();
    dt_ctx = vmalloc(sizeof(dt_ctx_t));
    dt_ctx->head = root_node;
    dt_ctx->phandle_map = uintmap_alloc(64);
    get_fdt_node(&fdt_ctx, dtb_tree, dt_ctx, root_node, true);

    dtb_post_process(dt_ctx, root_node);

    dtb_discover(dt_ctx, root_node);
}

static int dtb_query_name_internal(const char** path_list,
                                   dt_node_t* node,
                                   discovery_dtb_ctx_t* disc_ctx) {
    if (*path_list == NULL) {
        // Found node
        disc_ctx->dt_node = node;
        return 0;
    }

    dt_node_t* child_node;
    FOR_LLIST(node->children, child_node)
        if (strcmp(child_node->name, path_list[0]) == 0) {
            return dtb_query_name_internal(&path_list[1], child_node, disc_ctx);
        }
    END_FOR_LLIST()

    return 1;
}

int dtb_query_name(const char* dev, discovery_dtb_ctx_t* disc_ctx) {

    ASSERT(disc_ctx != NULL);

    const char** path_list = path_parser(dev, '/');

    int res = dtb_query_name_internal(path_list, dt_ctx->head, disc_ctx);

    free_path_list(path_list);

    if (res == 0) {
        disc_ctx->dt_ctx = dt_ctx;
    } else {
        disc_ctx->dt_ctx = NULL;
        disc_ctx->dt_node = NULL;
    }

    return res;
}

