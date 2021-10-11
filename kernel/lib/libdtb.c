
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/vmalloc.h"

#include "stdlib/string.h"
#include "stdlib/bitutils.h"


char* fdt_get_string(fdt_header_t* header, uint8_t* dtbmem, uint32_t stroff) {
    ASSERT(header != NULL);
    
    return (char*)&dtbmem[header->off_dt_strings + stroff];
}

static uint64_t dt_block_prop_size(fdt_property_t* prop, fdt_node_t* node, fdt_ctx_t* fdt_ctx) {

    uint64_t block_size = 0;

    char* name = fdt_get_string(fdt_ctx->header, fdt_ctx->fdt, prop->nameoff);

    uint64_t namelen = strnlen(name, 128);
    ASSERT(namelen != 128);

    if ((strncmp(name, "compatible", namelen+1) == 0) ||
        (strncmp(name, "model", namelen+1) == 0)) {
        block_size += namelen + 1; // Only need to store the name
    } else if ((strncmp(name, "phandle", namelen+1) == 0) ||
               (strncmp(name, "#address-cells", namelen+1) == 0) ||
               (strncmp(name, "#size-cells", namelen+1) == 0)) {
        // No need to allocate extra memory
    } else if (strncmp(name, "reg", namelen+1) == 0) {
        // Assume a worst case size of one reg entry per byte
        block_size += sizeof(dt_prop_reg_entry_t) * prop->data_len;
    } else if (strncmp(name, "ranges", namelen+1) == 0) {
        // Assume a worst case size of one range entry per three bytes
        block_size += sizeof(dt_prop_range_entry_t) * (prop->data_len / 3);
    } else if (strncmp(name, "dma-ranges", namelen+1) == 0) {
        // Assume a worst case size of one dma-range entry per three bytes
        block_size += sizeof(dt_prop_dma_range_entry_t) * (prop->data_len / 3);
    } else {
        // Assume a generic property
        block_size += sizeof(dt_prop_generic_t);
        block_size += namelen + 1;
        block_size += prop->data_len;
    }

    return block_size;
}

static uint64_t dt_block_node_size(fdt_node_t* base_node, fdt_ctx_t* fdt_ctx) {

    uint64_t block_size = 0;

    block_size += sizeof(dt_node_t); // Need one node structure
    if (base_node->name != NULL) {
        block_size += strnlen(base_node->name, 128) + 2; // Name plus 2 null characters (name and address)
    }

    for (int prop_idx = 0; prop_idx < base_node->num_properties; prop_idx++) {
        ASSERT(prop_idx < fdt_ctx->num_properties);
        fdt_property_t* prop = &fdt_ctx->property_list[base_node->properties_list[prop_idx]];
        block_size += dt_block_prop_size(prop, base_node, fdt_ctx);
    }

    for (int node_idx = 0; node_idx < base_node->num_children; node_idx++) {
        ASSERT(node_idx < fdt_ctx->num_nodes);
        fdt_node_t* child = &fdt_ctx->node_list[base_node->children_list[node_idx]];
        block_size += dt_block_node_size(child, fdt_ctx);
    }

    return block_size;
}

static uint64_t dt_build_copy_mem(void* mem, uint64_t len, void* block_data,
                                  uint64_t* block_free_offset, uint64_t block_size) {

    ASSERT((*block_free_offset + len) <= block_size);

    memcpy(block_data + (*block_free_offset), mem, len);

    uint64_t old_offset = *block_free_offset;
    *block_free_offset += len;
    return old_offset;
}

static int dt_build_props(dt_node_t* node, fdt_node_t* fdt_node, void* block_data,
                          uint64_t* block_free_offset, uint64_t block_size,
                          fdt_ctx_t* fdt_ctx) {

    node->num_properties = fdt_node->num_properties;

    // Set the default value of address and size cells
    node->cells.addr = 2;
    node->cells.size = 1;

    // Find the #adress-cells and #size-cells fields first,
    // since these are necessary to parse other fields
    for (uint64_t prop_idx = 0; prop_idx < node->num_properties; prop_idx++) {
        fdt_property_t* fdt_prop = &fdt_ctx->property_list[fdt_node->properties_list[prop_idx]];
        char* name = fdt_get_string(fdt_ctx->header, fdt_ctx->fdt, fdt_prop->nameoff);
        uint64_t name_len = strnlen(name, 128);
        ASSERT(name_len != 128);

        if (strncmp(name, "#address-cells", name_len+1) == 0) {
            uint32_t cells = fdt_prop->data_ptr[0] << 24 |
                             fdt_prop->data_ptr[1] << 16 |
                             fdt_prop->data_ptr[2] << 8 |
                             fdt_prop->data_ptr[3];
            node->cells.addr = cells;
        } else if (strncmp(name, "#size-cells", name_len+1) == 0) {
            uint32_t cells = fdt_prop->data_ptr[0] << 24 |
                             fdt_prop->data_ptr[1] << 16 |
                             fdt_prop->data_ptr[2] << 8 |
                             fdt_prop->data_ptr[3];
            node->cells.size = cells;
        }
    }

    // Allocate a temporary list of generic properties. This will be copied into the
    // node later
    uint64_t* tmp_property_list = vmalloc(sizeof(uint64_t) * fdt_node->num_properties);
    uint64_t gen_property_count = 0;

    // Repeat the search, this time with valid cell properties
    for (uint64_t prop_idx = 0; prop_idx < node->num_properties; prop_idx++) {
        fdt_property_t* fdt_prop = &fdt_ctx->property_list[fdt_node->properties_list[prop_idx]];
        char* name = fdt_get_string(fdt_ctx->header, fdt_ctx->fdt, fdt_prop->nameoff);
        uint64_t name_len = strnlen(name, 128);
        ASSERT(name_len != 128);

        if (strncmp(name, "compatible", name_len+1) == 0) {
            node->compat.compat_off =
                dt_build_copy_mem(fdt_prop->data_ptr, fdt_prop->data_len, block_data,
                                  block_free_offset, block_size);
            node->std_properties_mask |= DT_PROP_COMPAT;

        } else if (strncmp(name, "model", name_len+1) == 0) {
            node->model.model_off =
                dt_build_copy_mem(fdt_prop->data_ptr, fdt_prop->data_len, block_data,
                                  block_free_offset, block_size);
            node->std_properties_mask |= DT_PROP_MODEL;

        } else if (strncmp(name, "phandle", name_len+1) == 0) {
            uint32_t phandle = fdt_prop->data_ptr[0] << 24 |
                               fdt_prop->data_ptr[1] << 16 |
                               fdt_prop->data_ptr[2] << 8 |
                               fdt_prop->data_ptr[3];
            node->phandle.phandle = phandle;
            node->std_properties_mask |= DT_PROP_PHANDLE;

        } else if (strncmp(name, "reg", name_len+1) == 0) {
            uint64_t entry_size = node->parent_cells.addr + node->parent_cells.size;
            ASSERT(fdt_prop->data_len % entry_size == 0);

            uint64_t num_entries = fdt_prop->data_len / (entry_size * 4);

            // Allocate memory for each entry
            dt_prop_reg_entry_t* entry_ptr = block_data + *block_free_offset;
            node->reg.reg_entries_off = *block_free_offset;
            node->reg.num_regs = num_entries;
            *block_free_offset += sizeof(dt_prop_reg_entry_t) * num_entries;
            ASSERT(*block_free_offset <= block_size);

            // Build each entry
            for (uint64_t idx = 0; idx < num_entries; idx++) {
                uint8_t* reg_ptr = fdt_prop->data_ptr + (idx * entry_size * 4);

                uint64_t addr = 0;
                for (uint64_t cell_idx = 0; cell_idx < node->parent_cells.addr; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    addr = (addr << 32) | cell;
                    reg_ptr += 4;
                }

                uint64_t size = 0;
                for (uint64_t cell_idx = 0; cell_idx < node->parent_cells.addr; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    size = (size << 32) | (cell);
                    reg_ptr += 4;
                }

                entry_ptr[idx].addr = addr;
                entry_ptr[idx].size = size;
            }

            node->std_properties_mask |= DT_PROP_REG;

        } else if (strncmp(name, "ranges", name_len+1) == 0) {
            uint64_t entry_size = node->cells.addr + node->parent_cells.addr + node->cells.size;
            ASSERT(fdt_prop->data_len % entry_size == 0);

            uint64_t num_entries = fdt_prop->data_len / (entry_size * 4);

            // Allocate memory for each entry
            dt_prop_range_entry_t* entry_ptr = block_data + *block_free_offset;
            node->ranges.range_entries_off = *block_free_offset;
            node->ranges.num_ranges = num_entries;
            *block_free_offset += sizeof(dt_prop_range_entry_t) * num_entries;
            ASSERT(*block_free_offset <= block_size);

            // Build each entry
            for (uint64_t idx = 0; idx < num_entries; idx++) {
                uint8_t* reg_ptr = fdt_prop->data_ptr + (idx * entry_size * 4);

                uint64_t child_addr_cells = node->cells.addr;
                uint64_t pci_hi_addr = 0;
                if (child_addr_cells == 3) {
                    // For three u32 cells, use the pci_hi_addr field
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    pci_hi_addr = cell;
                    reg_ptr += 4;
                    child_addr_cells = 2;
                }

                uint64_t child_addr = 0;
                for (uint64_t cell_idx = 0; cell_idx < child_addr_cells; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    child_addr = (child_addr << 32) | cell;
                    reg_ptr += 4;
                }

                uint64_t parent_addr = 0;
                for (uint64_t cell_idx = 0; cell_idx < node->parent_cells.addr; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    parent_addr = (parent_addr << 32) | cell;
                    reg_ptr += 4;
                }

                uint64_t size = 0;
                for (uint64_t cell_idx = 0; cell_idx < node->cells.size; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    size = (size << 32) | (cell);
                    reg_ptr += 4;
                }

                entry_ptr[idx].child_addr = child_addr;
                entry_ptr[idx].pci_hi_addr = pci_hi_addr;
                entry_ptr[idx].parent_addr = parent_addr;
                entry_ptr[idx].size = size;
            }

            node->std_properties_mask |= DT_PROP_RANGES;

        } else if (strncmp(name, "dma-ranges", name_len+1) == 0) {
            uint64_t entry_size = node->cells.addr + node->parent_cells.addr + node->cells.size;
            ASSERT(fdt_prop->data_len % entry_size == 0);

            uint64_t num_entries = fdt_prop->data_len / (entry_size * 4);

            // Allocate memory for each entry
            dt_prop_dma_range_entry_t* entry_ptr = block_data + *block_free_offset;
            node->dma_ranges.dma_range_entries_off = *block_free_offset;
            node->dma_ranges.num_dma_ranges = num_entries;
            *block_free_offset += sizeof(dt_prop_dma_range_entry_t) * num_entries;
            ASSERT(*block_free_offset <= block_size);

            // Build each entry
            for (uint64_t idx = 0; idx < num_entries; idx++) {
                uint8_t* reg_ptr = fdt_prop->data_ptr + (idx * entry_size * 4);

                uint64_t child_addr_cells = node->cells.addr;
                uint64_t pci_hi_addr = 0;
                if (child_addr_cells == 3) {
                    // For three u32 cells, use the pci_hi_addr field
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    pci_hi_addr = cell;
                    reg_ptr += 4;
                    child_addr_cells = 2;
                }

                uint64_t child_addr = 0;
                for (uint64_t cell_idx = 0; cell_idx < child_addr_cells; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    child_addr = (child_addr << 32) | cell;
                    reg_ptr += 4;
                }

                uint64_t parent_addr = 0;
                for (uint64_t cell_idx = 0; cell_idx < node->parent_cells.addr; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    parent_addr = (parent_addr << 32) | cell;
                    reg_ptr += 4;
                }

                uint64_t size = 0;
                for (uint64_t cell_idx = 0; cell_idx < node->cells.addr; cell_idx++) {
                    uint32_t cell = (reg_ptr[0] << 24) |
                                    (reg_ptr[1] << 16) |
                                    (reg_ptr[2] << 8) |
                                     reg_ptr[3];
                    size = (size << 32) | (cell);
                    reg_ptr += 4;
                }

                entry_ptr[idx].child_addr = child_addr;
                entry_ptr[idx].pci_hi_addr = pci_hi_addr;
                entry_ptr[idx].parent_addr = parent_addr;
                entry_ptr[idx].size = size;
            }

            node->std_properties_mask |= DT_PROP_DMA_RANGES;
        } else if ((strncmp(name, "#address-cells", name_len+1) == 0) ||
                   (strncmp(name, "#size-cells", name_len+1) == 0)) {
            // Skip. Already handled
        } else {
            // Generic property. Copy the name and data directly
            dt_prop_generic_t* prop_ptr = block_data + *block_free_offset;
            tmp_property_list[gen_property_count] = *block_free_offset;
            gen_property_count++;
            *block_free_offset += sizeof(dt_prop_generic_t);
            ASSERT(*block_free_offset <= block_size);

            prop_ptr->data_off = dt_build_copy_mem(fdt_prop->data_ptr, fdt_prop->data_len, block_data,
                                                   block_free_offset, block_size);
            prop_ptr->data_len = fdt_prop->data_len;

            prop_ptr->name_off = dt_build_copy_mem(name, name_len+1, block_data,
                                                   block_free_offset, block_size);
        }
    }

    // Copy the tmp_property_list into the block structure
    uint64_t* property_list = block_data + *block_free_offset;
    node->num_properties = gen_property_count;
    if (gen_property_count > 0) {
        node->property_list_off = *block_free_offset;
        *block_free_offset += sizeof(uint64_t) * gen_property_count;
        ASSERT(*block_free_offset <= block_size);
        memcpy(property_list, tmp_property_list, sizeof(uint64_t) * gen_property_count);
    } else {
        node->property_list_off = 0;
    }

    return 0;
}

static int dt_build_node(dt_node_t* node, fdt_node_t* fdt_node, dt_prop_cells_t* parent_cells,
                         void* block_data, uint64_t* block_free_offset, uint64_t block_size,
                         fdt_ctx_t* fdt_ctx) {
    
    // Copy the node name
    uint64_t name_len = 0;
    if (fdt_node->name != NULL) {
        name_len = strnlen(fdt_node->name, 128) + 1;
        ASSERT(name_len != 128);

        char* name_copy = vmalloc(name_len);
        strncpy(name_copy, fdt_node->name, name_len);

        // Split the name based on a @ character, if present
        uint64_t idx = 0;
        bool have_address = false;
        char* address_str;
        while (name_copy[idx] != '\0') {
            char* char_ptr = &name_copy[idx];
            if (*char_ptr == '@') {
                have_address = true;
                *char_ptr = '\0';
                address_str = char_ptr + 1;
                name_len = idx + 1;
                break;
            }
            idx++;
        }

        if (have_address) {
            bool valid_address;
            node->address = hextou64(address_str, 16, &valid_address);
            ASSERT(valid_address);
        }

        node->name_off = dt_build_copy_mem(name_copy, name_len, block_data,
                                        block_free_offset, block_size);
        vfree(name_copy);

    } else {
        node->name_off = dt_build_copy_mem("", 1, block_data,
                                           block_free_offset, block_size);
    }

    node->parent_cells = *parent_cells;
    dt_build_props(node, fdt_node, block_data, block_free_offset, block_size, fdt_ctx);

    // Allocate memory for the node offset list
    node->num_nodes = fdt_node->num_children;
    uint64_t node_off_list_len = sizeof(uint64_t) * node->num_nodes;
    uint64_t* node_off_list;
    node_off_list = block_data + *block_free_offset;
    node->node_list_off = *block_free_offset;
    *block_free_offset += node_off_list_len;
    ASSERT(*block_free_offset <= block_size);

    for (uint64_t node_idx = 0; node_idx < fdt_node->num_children; node_idx++) {
        node_off_list[node_idx] = *block_free_offset;
        dt_node_t* new_node = block_data + *block_free_offset;

        *block_free_offset += sizeof(dt_node_t);
        ASSERT(*block_free_offset <= block_size);

        fdt_node_t* fdt_child_node = &fdt_ctx->node_list[fdt_node->children_list[node_idx]];
        dt_build_node(new_node, fdt_child_node, &node->cells,
                      block_data, block_free_offset, block_size,
                      fdt_ctx);
    }

    return 0;
}

dt_block_t* dt_build_block(fdt_node_t* base_fdt_node, fdt_ctx_t* fdt_ctx, dt_prop_cells_t* parent_cells) {

    uint64_t block_size;
    dt_block_t* block_out;

    // Determine how much memory is required to allocate a block
    block_size = dt_block_node_size(base_fdt_node, fdt_ctx);

    // Allocate memory for the block
    block_out = vmalloc(block_size + sizeof(dt_block_t));
    if (block_out == NULL) {
        return block_out;
    }

    // Build the block

    // Hold the position of the next free byte in the block
    uint64_t block_free_offset = 0; 

    block_free_offset += sizeof(dt_node_t);

    dt_node_t* base_node = (dt_node_t*)block_out->data;
    dt_prop_cells_t default_cells = {
        .addr = 2,
        .size = 1
    };

    if (parent_cells != NULL) {
        default_cells = *parent_cells;
    }

    int res = dt_build_node(base_node, base_fdt_node, &default_cells,
                            block_out->data, &block_free_offset, block_size,
                            fdt_ctx);
    
    if (res != 0) {
        vfree(block_out);
        return NULL;
    }

    ASSERT(block_free_offset <= block_size);

    block_out->len = block_free_offset;

    return block_out;
}