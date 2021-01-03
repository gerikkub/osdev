
#include <stdint.h>
#include <string.h>

#include "system/system_lib.h"
#include "system/system_msg.h"
#include "system/system_console.h"
#include "system/system_assert.h"
#include "system/system_dtb.h"
#include "system/system_malloc.h"

#include "include/k_syscall.h"
#include "include/k_messages.h"
#include "include/k_modules.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

#define MAX_DTB_PROPERTIES 1024
#define MAX_DTB_NODES 128
#define MAX_DTB_NODE_PROPERTIES 32
#define MAX_DTB_NODE_CHILDREN 64
#define MAX_DTB_PROPERTY_LEN 64

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
    union {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        struct {
            char data[MAX_DTB_PROPERTY_LEN];
            int64_t len;
        } str;
    } data;
    fdt_property_type_t type;

    uint32_t nameoff;
} fdt_property_t;

typedef struct {
    char name[MAX_DTB_NODE_NAME];
    bool name_valid;
    uint64_t properties_list[MAX_DTB_NODE_PROPERTIES];
    int64_t num_properties;
    uint64_t children_list[MAX_DTB_NODE_CHILDREN];
    int64_t num_children;
} fdt_node_t;

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

char* get_fdt_string(fdt_header_t* header, uint8_t* dtbmem, uint32_t stroff) {
    SYS_ASSERT(header != NULL);
    
    return (char*)&dtbmem[header->off_dt_strings + stroff];
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
    switch (prop_len) {
        case 0:
            property->type = FDT_PROP_TYPE_EMPTY;
            break;
        case 1:
            property->type = FDT_PROP_TYPE_U8;
            property->data.u8 = *prop_value_ptr;
            break;
        case 2:
            property->type = FDT_PROP_TYPE_U16;
            property->data.u16 = en_swap_16(*(uint16_t*)prop_value_ptr);
            break;
        case 4:
            property->type = FDT_PROP_TYPE_U32;
            property->data.u32 = en_swap_32(*(uint32_t*)prop_value_ptr);
            break;
        case 8:
            property->type = FDT_PROP_TYPE_U64;
            property->data.u64 = en_swap_64(*(uint64_t*)prop_value_ptr);
            break;
        default:
            property->type = FDT_PROP_TYPE_STR;
            if (prop_len < MAX_DTB_PROPERTY_LEN) {
                memcpy(property->data.str.data, prop_value_ptr, prop_len);
                property->data.str.len = prop_len;
            } else {
                // Not enough storage space
                property->data.str.len = -1;
            }
            break;
    }

    // Skip over padding added to the property
    // to align the next token to a uint32_t
    uintptr_t prop_padded = (uintptr_t)(prop_value_ptr + prop_len);
    prop_padded = (prop_padded + sizeof(uint32_t) - 1) & (~(sizeof(uint32_t) - 1));

    return (uint8_t*)prop_padded;
}

uint8_t* get_fdt_node(uint8_t* dtb_ptr, fdt_node_t* node) {
    SYS_ASSERT(dtb_ptr != NULL);
    SYS_ASSERT(node != NULL);

    uint32_t* token_ptr = (uint32_t*)dtb_ptr;
    uint32_t token = en_swap_32(*token_ptr);
    SYS_ASSERT(token == FDT_TOKEN_BEGIN_NODE);

    token_ptr++;

    // Read in the name as a null terminated string
    char* node_name_ptr = (char*)token_ptr;
    strncpy(node->name, node_name_ptr, MAX_DTB_NODE_NAME);

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

void print_node_padding(uint64_t padding) {
    for (uint64_t idx = 0; idx < padding; idx++) {
        console_putc(' ');
    }
}

void print_node(uint8_t* dtbmem, fdt_header_t* header, fdt_node_t* node, uint64_t padding) {

    SYS_ASSERT(dtbmem != NULL);
    SYS_ASSERT(header != NULL);
    SYS_ASSERT(node != NULL);

    print_node_padding(padding);
    console_printf("NODE: %s %c\n", node->name, node->name_valid ? 'v' : 'x');

    // Print all properties
    uint64_t sub_padding = padding + 2;
    for (uint64_t idx = 0; idx < node->num_properties; idx++) {
        fdt_property_t* prop = &s_fdt_property_list[node->properties_list[idx]];
        char* prop_name = get_fdt_string(header, dtbmem, prop->nameoff);

        /*if (strncmp(prop_name, "compatible", 10) != 0) {
            continue;
        }*/
        print_node_padding(sub_padding);
        console_printf("PROP %s: \n", prop_name);
        print_node_padding(sub_padding + 2);
        switch(prop->type) {
            case FDT_PROP_TYPE_UNKNOWN:
                console_printf("UNKNOWN\n");
                break;
            case FDT_PROP_TYPE_EMPTY:
                console_printf("EMPTY\n");
                break;
            case FDT_PROP_TYPE_U8:
                console_printf("u8: %2x\n", prop->data.u8);
                break;
            case FDT_PROP_TYPE_U16:
                console_printf("u16: %4x\n", prop->data.u16);
                break;
            case FDT_PROP_TYPE_U32:
                console_printf("u32: %8x\n", prop->data.u32);
                break;
            case FDT_PROP_TYPE_U64:
                console_printf("u64: %16x\n", prop->data.u64);
                break;
            case FDT_PROP_TYPE_STR:
                console_printf("str: %s OR ", prop->data.str.data);
                for (int i = 0; i < prop->data.str.len; i++) {
                    console_printf("%2x ", prop->data.str.data[i]);
                }
                console_printf("\n");
                break;
            default:
                SYS_ASSERT(0);
        }
    }

    // Print all children nodes
    for (uint64_t idx = 0; idx < node->num_children; idx++ ) {
        fdt_node_t* child = &s_fdt_node_list[node->children_list[idx]];
        print_node(dtbmem, header, child, padding + 2);
    }

    console_flush();
}

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

    //print_node(devicetree, &header, root_node, 0);

    //console_printf("DONE\n");
    //console_flush();

    int64_t idx;
    for (idx = 0; idx < root_node->num_children; idx++) {
        fdt_node_t* child_node = &s_fdt_node_list[root_node->children_list[idx]];

        // Check if a compatiblity property is present
        int64_t prop_idx;
        fdt_property_t* prop;
        for (prop_idx = 0; prop_idx < child_node->num_properties; prop_idx++) {
            prop = &s_fdt_property_list[child_node->properties_list[prop_idx]];
            char* prop_name = get_fdt_string(&header, devicetree, prop->nameoff);
            if (strncmp(prop_name, "compatible", strlen("compatible")+1) == 0) {
                break;
            }
        }
        if (prop_idx == child_node->num_properties) {
            continue;
        }
        if (prop->type != FDT_PROP_TYPE_STR) {
            continue;
        }

        // The property data may not have a nul terminator
        char wellformed_compat[MAX_DTB_PROPERTY_LEN + 1] = {0};
        memcpy(wellformed_compat, prop->data.str.data, prop->data.str.len);

        // Found a compatiblity node. Start a module
        int64_t mod_tid = system_startmod_compat(wellformed_compat);

        if (mod_tid > 0) {
            // Got a handle to a module. Now send it
            // a DTB message

            system_msg_memory_t dtb_msg = {
                .type = MSG_TYPE_MEMORY,
                .flags = 0,
                .dst = mod_tid,
                .src = my_tid,
                .port = MOD_GENERIC_DTB,
                .ptr = (uintptr_t)child_node->name,
                .len = MAX_DTB_NODE_NAME,
                .payload = {0}
            };

            system_send_msg((system_msg_t*)&dtb_msg);
        }
    }


    while (1) {
        system_recv_msg();
    }
}