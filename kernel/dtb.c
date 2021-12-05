
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/drivers.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"

#include "include/k_syscall.h"
#include "include/k_messages.h"
#include "include/k_modules.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

#define MAX_DTB_PROPERTIES 1024
#define MAX_DTB_NODES 128
#define MAX_DTB_NODE_PROPERTIES 32
#define MAX_DTB_NODE_CHILDREN 64
#define MAX_DTB_PROPERTY_LEN 128
#define MAX_DTB_NODE_NAME 64
#define MAX_DTB_SIZE 1048576

static fdt_property_t s_fdt_property_list[MAX_DTB_PROPERTIES] = {0};
static int64_t s_fdt_property_idx = 0;

static fdt_node_t s_fdt_node_list[MAX_DTB_NODES] = {0};
static int64_t s_fdt_node_idx = 0;

static dt_block_t* s_full_dtb;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint64_t size;
} fdt_reserve_entry_t;

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

uint8_t* get_fdt_property(uint8_t* dtb_ptr, fdt_property_t* property) {
    ASSERT(dtb_ptr != NULL);
    ASSERT(property != NULL);

    fdt_property_token_t token = *(fdt_property_token_t*)dtb_ptr;


    ASSERT(en_swap_32(token.token) == FDT_TOKEN_PROP);
    uint8_t* prop_value_ptr = dtb_ptr + sizeof(fdt_property_token_t);
    uint32_t prop_len = en_swap_32(token.len);

    property->nameoff = en_swap_32(token.nameoff);
    property->data_len = prop_len;
    property->data_ptr = prop_value_ptr;

    // Skip over padding added to the property
    // to align the next token to a uint32_t
    uintptr_t prop_padded = (uintptr_t)(prop_value_ptr + prop_len);
    prop_padded = (prop_padded + sizeof(uint32_t) - 1) & (~(sizeof(uint32_t) - 1));

    return (uint8_t*)prop_padded;
}

uint8_t* get_fdt_node(uint8_t* dtb_ptr, fdt_node_t* node) {
    ASSERT(dtb_ptr != NULL);
    ASSERT(node != NULL);

    uint32_t* token_ptr = (uint32_t*)dtb_ptr;
    uint32_t token = en_swap_32(*token_ptr);
    ASSERT(token == FDT_TOKEN_BEGIN_NODE);

    token_ptr++;

    // Read in the name as a null terminated string
    char* node_name_ptr = (char*)token_ptr;
    node->name = vmalloc(MAX_DTB_NODE_NAME);
    if (node->name == NULL) {
        node_name_ptr[0] = '\0';
    } else {
        strncpy(node->name, node_name_ptr, MAX_DTB_NODE_NAME);
    }

    uint64_t node_name_len = 0;
    while (*node_name_ptr != '\0') {
        node_name_len++;
        node_name_ptr++;
    }

    // Add an extra byte for the null terminator
    node_name_len++;
    node_name_ptr++;

    if (node_name_len >= MAX_DTB_NODE_NAME) {
        node->name_valid = false;
    } else {
        node->name_valid = true;
    }

    // Optionally skip over padding added to the name
    // to align the next token to a uint32_t
    uintptr_t node_name_padded = (uintptr_t)node_name_ptr;
    node_name_padded = (node_name_padded + sizeof(uint32_t) - 1) & (~(sizeof(uint32_t) - 1));

    // Continue with token ptr to parse the rest
    // of the node
    token_ptr = (uint32_t*)node_name_padded;

    node->properties_list = vmalloc(sizeof(uint64_t) * MAX_DTB_NODE_PROPERTIES);
    node->num_properties = 0;

    // Parse all properties first. These are guaranteed to be
    // before child nodes
    while (is_token_value_or_nop(*token_ptr, FDT_TOKEN_PROP)) {
        if (is_token_nop(*token_ptr)) {
            token_ptr++;
        } else {
            ASSERT(s_fdt_property_idx < MAX_DTB_PROPERTIES);
            ASSERT(node->num_properties < MAX_DTB_NODE_PROPERTIES);
            node->properties_list[node->num_properties] = s_fdt_property_idx;
            node->num_properties++;
            // Allocate a property
            fdt_property_t* prop = &s_fdt_property_list[s_fdt_property_idx];
            s_fdt_property_idx++;

            token_ptr = (uint32_t*)get_fdt_property((uint8_t*)token_ptr, prop);
        }
    }

    node->children_list = vmalloc(sizeof(uint64_t) * MAX_DTB_NODE_CHILDREN);

    // Parse all child nodes
    while (is_token_value_or_nop(*token_ptr, FDT_TOKEN_BEGIN_NODE)) {
        if (is_token_nop(*token_ptr)) {
            token_ptr++;
        } else {
            ASSERT(s_fdt_node_idx < MAX_DTB_NODES);
            ASSERT(node->num_children < MAX_DTB_NODE_CHILDREN);
            node->children_list[node->num_children] = s_fdt_node_idx;
            node->num_children++;
            // Allocate a node
            fdt_node_t* node = &s_fdt_node_list[s_fdt_node_idx];
            s_fdt_node_idx++;

            // Recurse
            token_ptr = (uint32_t*)get_fdt_node((uint8_t*)token_ptr, node);
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

void print_node_padding(uint64_t padding) {
    for (uint64_t idx = 0; idx < padding; idx++) {
        console_putc(' ');
    }
}

void print_node(uint8_t* dtbmem, fdt_header_t* header, fdt_node_t* node, uint64_t padding) {

    ASSERT(dtbmem != NULL);
    ASSERT(header != NULL);
    ASSERT(node != NULL);

    print_node_padding(padding);
    console_printf("NODE: %s %c\n", node->name, node->name_valid ? 'v' : 'x');

    // Print all properties
    uint64_t sub_padding = padding + 2;
    for (uint64_t idx = 0; idx < node->num_properties; idx++) {
        fdt_property_t* prop = &s_fdt_property_list[node->properties_list[idx]];
        char* prop_name = fdt_get_string(header, dtbmem, prop->nameoff);

        print_node_padding(sub_padding);
        console_printf("PROP %s: ", prop_name);
        //print_node_padding(sub_padding + 2);

        if (strncmp(prop_name, "compatible", 10) == 0) {
            console_printf("%s", prop->data_ptr);
        } else {

            for (uint64_t i = 0; i < prop->data_len; i++) {
                console_printf("%2x ", prop->data_ptr[i]);
            }
        }

        console_printf("\n");
    }

    // Print all children nodes
    for (uint64_t idx = 0; idx < node->num_children; idx++ ) {
        fdt_node_t* child = &s_fdt_node_list[node->children_list[idx]];
        print_node(dtbmem, header, child, padding + 2);
    }

    console_flush();
}

static void dtb_discover(uint8_t* dtbmem, fdt_node_t* node, fdt_header_t* header, fdt_ctx_t* ctx, dt_prop_cells_t* parent_cells) {

    dt_prop_cells_t this_cells = {
        .addr = 2,
        .size = 1
    };

    for (uint64_t prop_idx = 0; prop_idx < node->num_properties; prop_idx++) {

        fdt_property_t* prop = &s_fdt_property_list[node->properties_list[prop_idx]];
        char* prop_name = fdt_get_string(header, dtbmem, prop->nameoff);

        if (strncmp(prop_name, "compatible", 10) == 0) {
            discovery_register_t disc_reg = {
                .type = DRIVER_DISCOVERY_DTB,
                .dtb = {
                    .compat = (char*)prop->data_ptr
                },
                .ctxfunc = NULL
            };

            if (discovery_have_driver(&disc_reg)) {
                // print_node(dtbmem, header, node, 2);

                dt_block_t* disc_block = dt_build_block(node, ctx, parent_cells);
                discovery_dtb_ctx_t disc_ctx = {
                    .block = disc_block
                };

                discover_driver(&disc_reg, &disc_ctx);

                // Block free'd by the driver
            } else {
                //console_printf("No driver for %s\n", prop->data_ptr);
            }
        } else if (strncmp(prop_name, "#size-cells", 11) == 0) {
            this_cells.size = prop->data_ptr[0] << 24 |
                              prop->data_ptr[1] << 16 |
                              prop->data_ptr[2] << 8 |
                              prop->data_ptr[3];
        } else if (strncmp(prop_name, "#address-cells", 13) == 0) {
            this_cells.addr = prop->data_ptr[0] << 24 |
                              prop->data_ptr[1] << 16 |
                              prop->data_ptr[2] << 8 |
                              prop->data_ptr[3];
        }
    }

    for (uint64_t child_idx = 0; child_idx < node->num_children; child_idx++) {
        fdt_node_t* child_node = &s_fdt_node_list[node->children_list[child_idx]];
        dtb_discover(dtbmem, child_node, header, ctx, &this_cells);
    }
}

void dtb_init(void) {

    void* header_phy_ptr = 0;
    uint8_t* devicetree = (uint8_t*)PHY_TO_KSPACE(header_phy_ptr);

    memspace_map_phy_kernel(header_phy_ptr,
                            devicetree,
                            MAX_DTB_SIZE, MEMSPACE_FLAG_PERM_KRO);
    memspace_update_kernel_vmem();

    fdt_header_t header;
    get_fdt_header(devicetree, &header);

    ASSERT(header.totalsize <= MAX_DTB_SIZE);

    uint8_t* dtb_tree = (devicetree + header.off_dt_struct);

    fdt_node_t* root_node;

    ASSERT(s_fdt_node_idx < MAX_DTB_NODES);
    root_node = &s_fdt_node_list[s_fdt_node_idx];
    s_fdt_node_idx++;

    get_fdt_node(dtb_tree, root_node);

    fdt_ctx_t fdt_ctx = {
        .fdt = devicetree,
        .header = &header,
        .property_list = s_fdt_property_list,
        .num_properties = s_fdt_property_idx,
        .node_list = s_fdt_node_list,
        .num_nodes = s_fdt_node_idx
    };

    s_full_dtb = dt_build_block(root_node, &fdt_ctx, NULL);
    ASSERT(s_full_dtb != NULL);

    // dt_block_t* block = dt_build_block(root_node, &fdt_ctx);

    // print_node(devicetree, &header, root_node, 2);

    dtb_discover(devicetree, root_node, &header, &fdt_ctx, NULL);
}

const dt_block_t* dtb_get_devicetree(void) {
    return s_full_dtb;
}