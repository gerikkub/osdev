
#include <stdint.h>
#include <string.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_dtb.h"
#include "system/lib/system_malloc.h"
#include "system/lib/libdtb.h"

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

static fdt_property_t s_fdt_property_list[MAX_DTB_PROPERTIES] = {0};
static int64_t s_fdt_property_idx = 0;

static fdt_node_t s_fdt_node_list[MAX_DTB_NODES] = {0};
static int64_t s_fdt_node_idx = 0;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint64_t size;
} fdt_reserve_entry_t;

void get_fdt_header(fdt_header_t* header_out) {
    const fdt_header_t* header = system_map_device(0, 1024);
    SYS_ASSERT(header != NULL);

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
    SYS_ASSERT(dtb_ptr != NULL);
    SYS_ASSERT(property != NULL);

    fdt_property_token_t token = *(fdt_property_token_t*)dtb_ptr;


    SYS_ASSERT(en_swap_32(token.token) == FDT_TOKEN_PROP);
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
    SYS_ASSERT(dtb_ptr != NULL);
    SYS_ASSERT(node != NULL);

    console_printf("get_fdt_node: %x %x\n", dtb_ptr, node);

    uint32_t* token_ptr = (uint32_t*)dtb_ptr;
    uint32_t token = en_swap_32(*token_ptr);
    SYS_ASSERT(token == FDT_TOKEN_BEGIN_NODE);

    token_ptr++;

    // Read in the name as a null terminated string
    char* node_name_ptr = (char*)token_ptr;
    node->name = malloc(MAX_DTB_NODE_NAME);
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

    node->properties_list = malloc(sizeof(uint64_t) * MAX_DTB_NODE_PROPERTIES);
    node->num_properties = 0;

    // Parse all properties first. These are guaranteed to be
    // before child nodes
    while (is_token_value_or_nop(*token_ptr, FDT_TOKEN_PROP)) {
        if (is_token_nop(*token_ptr)) {
            token_ptr++;
        } else {
            SYS_ASSERT(s_fdt_property_idx < MAX_DTB_PROPERTIES);
            SYS_ASSERT(node->num_properties < MAX_DTB_NODE_PROPERTIES);
            node->properties_list[node->num_properties] = s_fdt_property_idx;
            node->num_properties++;
            // Allocate a property
            fdt_property_t* prop = &s_fdt_property_list[s_fdt_property_idx];
            s_fdt_property_idx++;

            token_ptr = (uint32_t*)get_fdt_property((uint8_t*)token_ptr, prop);
        }
    }

    node->children_list = malloc(sizeof(uint64_t) * MAX_DTB_NODE_CHILDREN);

    // Parse all child nodes
    while (is_token_value_or_nop(*token_ptr, FDT_TOKEN_BEGIN_NODE)) {
        if (is_token_nop(*token_ptr)) {
            token_ptr++;
        } else {
            SYS_ASSERT(s_fdt_node_idx < MAX_DTB_NODES);
            SYS_ASSERT(node->num_children < MAX_DTB_NODE_CHILDREN);
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
    SYS_ASSERT(en_swap_32(*token_ptr) == FDT_TOKEN_END_NODE);
    token_ptr++;

    return (uint8_t*)token_ptr;
}

// void print_node_padding(uint64_t padding) {
//     for (uint64_t idx = 0; idx < padding; idx++) {
//         console_putc(' ');
//     }
// }

// void print_node(uint8_t* dtbmem, fdt_header_t* header, fdt_node_t* node, uint64_t padding) {

//     SYS_ASSERT(dtbmem != NULL);
//     SYS_ASSERT(header != NULL);
//     SYS_ASSERT(node != NULL);

//     print_node_padding(padding);
//     console_printf("NODE: %s %c\n", node->name, node->name_valid ? 'v' : 'x');

//     // Print all properties
//     uint64_t sub_padding = padding + 2;
//     for (uint64_t idx = 0; idx < node->num_properties; idx++) {
//         fdt_property_t* prop = &s_fdt_property_list[node->properties_list[idx]];
//         char* prop_name = get_fdt_string(header, dtbmem, prop->nameoff);

//         /*if (strncmp(prop_name, "compatible", 10) != 0) {
//             continue;
//         }*/
//         print_node_padding(sub_padding);
//         console_printf("PROP %s: \n", prop_name);
//         print_node_padding(sub_padding + 2);
//         /*
//         switch(prop->type) {
//             case FDT_PROP_TYPE_UNKNOWN:
//                 console_printf("UNKNOWN\n");
//                 break;
//             case FDT_PROP_TYPE_EMPTY:
//                 console_printf("EMPTY\n");
//                 break;
//             case FDT_PROP_TYPE_U8:
//                 console_printf("u8: %2x\n", prop->data.u8);
//                 break;
//             case FDT_PROP_TYPE_U16:
//                 console_printf("u16: %4x\n", prop->data.u16);
//                 break;
//             case FDT_PROP_TYPE_U32:
//                 console_printf("u32: %8x\n", prop->data.u32);
//                 break;
//             case FDT_PROP_TYPE_U64:
//                 console_printf("u64: %16x\n", prop->data.u64);
//                 break;
//             case FDT_PROP_TYPE_STR:
//                 console_printf("str: %s OR ", prop->data.str.data);
//                 for (int i = 0; i < prop->data.str.len; i++) {
//                     console_printf("%2x ", prop->data.str.data[i]);
//                 }
//                 console_printf("\n");
//                 break;
//             default:
//                 SYS_ASSERT(0);
//         }*/
//     }

//     // Print all children nodes
//     for (uint64_t idx = 0; idx < node->num_children; idx++ ) {
//         fdt_node_t* child = &s_fdt_node_list[node->children_list[idx]];
//         print_node(dtbmem, header, child, padding + 2);
//     }

//     console_flush();
// }

void main(uint64_t my_tid, module_ctx_t* ctx) {

    console_printf("DTB\n");
    console_flush();

    malloc_init();

    void* ptr_1 = malloc(30);
    void* ptr_2 = malloc(77);
    void* ptr_3 = malloc(90);

    console_printf("1: %x\n2: %x\n3: %x\n",
                   (uintptr_t)ptr_1,
                   (uintptr_t)ptr_2,
                   (uintptr_t)ptr_3);
    console_flush();
    
    free(ptr_1);

    void* ptr_4 = malloc(19);
    console_printf("\n4: %x\n", (uintptr_t)ptr_4);
    console_flush();

    fdt_header_t header;
    get_fdt_header(&header);

    console_printf("Got header\n");
    console_flush();

    uint8_t* devicetree = system_map_device(0, header.totalsize);
    SYS_ASSERT(devicetree != NULL);

    fdt_reserve_entry_t* res_entries = (fdt_reserve_entry_t*)(devicetree + header.off_mem_rsvmap);
    console_printf("Reserved Regions:");
    while (res_entries->address != 0 &&
           res_entries->size != 0) {

        console_printf(" %16X %X\n", en_swap_64(res_entries->address), en_swap_64(res_entries->size));
        res_entries++;
    }
    console_printf("\n");
    console_flush();

    uint8_t* dtb_tree = (devicetree + header.off_dt_struct);

    fdt_node_t* root_node;

    SYS_ASSERT(s_fdt_node_idx < MAX_DTB_NODES);
    root_node = &s_fdt_node_list[s_fdt_node_idx];
    s_fdt_node_idx++;

    get_fdt_node(dtb_tree, root_node);

    // print_node(devicetree, &header, root_node, 0);

    console_printf("DTB DONE\n");
    console_flush();

    fdt_ctx_t fdt_ctx = {
        .fdt = devicetree,
        .header = &header,
        .property_list = s_fdt_property_list,
        .num_properties = s_fdt_property_idx,
        .node_list = s_fdt_node_list,
        .num_nodes = s_fdt_node_idx
    };

    dt_block_t* block = dt_build_block(root_node, &fdt_ctx);

    int64_t idx;
    dt_node_t* dt_root_node = (dt_node_t*)block->data;
    SYS_ASSERT(dt_root_node->node_list_off < block->len);
    uint64_t* node_list = (uint64_t*)&block->data[dt_root_node->node_list_off];
    for (idx = 0; idx < dt_root_node->num_nodes; idx++) {
        uint64_t child_node_off = node_list[idx];
        SYS_ASSERT(child_node_off < block->len);
        dt_node_t* child_node = (dt_node_t*)&block->data[child_node_off];

        // Check if a compatiblity property is present
        if (!(child_node->std_properties_mask & DT_PROP_COMPAT)) {
            continue;
        }

        SYS_ASSERT(child_node->compat.compat_off < block->len);
        char* compat_str = (char*)&block->data[child_node->compat.compat_off];

        // Found a compatiblity node. Start a module
        int64_t mod_tid = system_startmod_compat(compat_str);

        if (mod_tid > 0) {
            // Got a handle to a module. Now send it
            // a DTB message

            block->node_off = child_node_off;

            module_ctx_t compat_ctx = {
                .startsel = MOD_STARTSEL_COMPAT
            };

            system_msg_memory_t dtb_msg = {
                .type = MSG_TYPE_MEMORY,
                .flags = 0,
                .dst = mod_tid,
                .src = my_tid,
                .port = MOD_GENERIC_CTX,
                .ptr = (uintptr_t)block,
                .len = sizeof(dt_block_t) + block->len,
                .payload = {MOD_STARTSEL_COMPAT}
            };

            SYS_ASSERT(sizeof(compat_ctx) <= sizeof(dtb_msg.payload));
            memcpy(dtb_msg.payload, &compat_ctx, sizeof(compat_ctx));

            system_send_msg((system_msg_t*)&dtb_msg);
        }
    }


    while (1) {
        system_recv_msg();
    }
}