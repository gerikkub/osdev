
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libdtb.h"

#include "stdlib/printf.h"

uintptr_t dt_map_addr_to_phy(dt_node_t* node, uintptr_t addr, bool* valid) {

    if (node->parent == NULL ||
        node->parent->prop_ranges == NULL) {

        *valid = true;
        return addr;
    }

    dt_prop_ranges_t* ranges = node->parent->prop_ranges;

    // Identity map if ranges is present but empty
    if (ranges->num_ranges == 0) {
        return dt_map_addr_to_phy(node->parent, addr, valid);
    }

    uintptr_t child_addr = 0;
    uint64_t child_size = 0;
    uintptr_t parent_addr = 0;

    uint64_t idx;
    for (idx = 0; idx < ranges->num_ranges; idx++) {
        dt_prop_range_entry_t* entry = &ranges->range_entries[idx];

        ASSERT(entry->addr_size > 0);
        ASSERT(entry->paddr_size > 0);
        ASSERT(entry->size_size > 0);
        if (entry->addr_size == 1) {
            child_addr = entry->addr_ptr[0];
        } else {
            child_addr = ((uintptr_t)entry->addr_ptr[entry->addr_size - 2] << 32) | 
                          (uintptr_t)entry->addr_ptr[entry->addr_size - 1];
        }

        if (entry->size_size == 1) {
            child_size = entry->size_ptr[0];
        } else {
            child_size = ((uintptr_t)entry->size_ptr[entry->size_size - 2] << 32) | 
                          (uintptr_t)entry->size_ptr[entry->size_size - 1];
        }

        if (entry->paddr_size == 1) {
            parent_addr = entry->paddr_ptr[0];
        } else {
            parent_addr = ((uintptr_t)entry->paddr_ptr[entry->paddr_size - 2] << 32) | 
                           (uintptr_t)entry->paddr_ptr[entry->paddr_size - 1];
        }

        if (addr >= child_addr &&
            addr < (child_addr + child_size)) {
            break;
        }
    }

    if (idx == ranges->num_ranges) {
        *valid = false;
        return 0;
    } else {
        uint64_t new_addr = (addr - child_addr) + parent_addr;
        return dt_map_addr_to_phy(node->parent, new_addr, valid);
    }
}

void dt_print_node_padding(uint64_t padding) {
    for (uint64_t idx = 0; idx < padding; idx++) {
        console_putc(' ');
    }
}

void dt_print_node(dt_ctx_t* ctx, dt_node_t* node, uint64_t padding, bool children) {

    ASSERT(ctx != NULL);
    ASSERT(node != NULL);

    dt_print_node_padding(padding);
    console_printf("%s", node->name);
    if (node->address != 0) {
        console_printf("@%x", node->address);
    }
    console_printf(" {\n");

    // Print all properties
    uint64_t sub_padding = padding + 2;

    dt_print_node_padding(sub_padding);
    console_printf("#address-cells: %d\n", node->prop_addr_cells->val);
    dt_print_node_padding(sub_padding);
    console_printf("#size-cells: %d\n", node->prop_size_cells->val);
    
    if (node->prop_compat) {
        dt_print_node_padding(sub_padding);
        console_printf("compatible: ");
        console_write_len(node->prop_compat->str, node->prop_compat->len);
        console_printf("\n");
    }

    if (node->prop_model) {
        dt_print_node_padding(sub_padding);
        console_printf("model: %s\n", node->prop_model->str);
    }

    if (node->prop_phandle) {
        dt_print_node_padding(sub_padding);
        console_printf("phandle: 0x%x\n", node->prop_phandle->val);
    }

    if (node->prop_reg) {
        dt_print_node_padding(sub_padding);
        console_printf("reg: <");
        for (uint64_t regs = 0; regs < node->prop_reg->num_regs; regs++) {
            for (uint64_t idx = 0; idx < node->prop_reg->reg_entries[regs].addr_size; idx++) {
                console_printf(" 0x%x", (uint64_t)node->prop_reg->reg_entries[regs].addr_ptr[idx]);
            }
            for (uint64_t idx = 0; idx < node->prop_reg->reg_entries[regs].size_size; idx++) {
                console_printf(" 0x%x", (uint64_t)node->prop_reg->reg_entries[regs].size_ptr[idx]);
            }
        }
        console_printf(" >\n");
    }

    if (node->prop_ranges) {
        dt_print_node_padding(sub_padding);
        console_printf("ranges: <");
        for (uint64_t ranges = 0; ranges < node->prop_ranges->num_ranges; ranges++) {
            for (uint64_t idx = 0; idx < node->prop_ranges->range_entries[ranges].addr_size; idx++) {
                console_printf(" 0x%x", (uint64_t)node->prop_ranges->range_entries[ranges].addr_ptr[idx]);
            }
            for (uint64_t idx = 0; idx < node->prop_ranges->range_entries[ranges].paddr_size; idx++) {
                console_printf(" 0x%x", (uint64_t)node->prop_ranges->range_entries[ranges].paddr_ptr[idx]);
            }
            for (uint64_t idx = 0; idx < node->prop_ranges->range_entries[ranges].size_size; idx++) {
                console_printf(" 0x%x", (uint64_t)node->prop_ranges->range_entries[ranges].size_ptr[idx]);
            }
        }
        console_printf(" >\n");
    }

    dt_prop_generic_t* prop;
    FOR_LLIST(node->properties, prop)
        dt_print_node_padding(sub_padding);
        console_printf("%s", prop->name);
        if (prop->data_len > 0) {
            console_printf(": <");
            if (prop->data_len % 4 == 0) {
                for (uint64_t idx = 0; idx < prop->data_len/4; idx++) {
                    console_printf(" 0x%8x", (uint64_t)en_swap_32(prop->data[idx]));
                }
            } else {
                uint8_t* data8 = (uint8_t*)prop->data;
                for (uint64_t idx = 0; idx < prop->data_len; idx++) {
                    console_printf(" 0x%8x", (uint64_t)data8[idx]);
                }
            }
            console_printf(" >\n");
        } else {
            console_printf(";\n");
        }
    END_FOR_LLIST()

    if (children) {
        dt_node_t* child_node;
        FOR_LLIST(node->children, child_node)
            dt_print_node(ctx, child_node, sub_padding, true);
        END_FOR_LLIST()
    }

    dt_print_node_padding(padding);
    console_printf("}\n");
}

