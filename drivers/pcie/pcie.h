
#pragma once

#include <stdint.h>

#include "kernel/lib/libpci.h"
#include "kernel/lib/libdtb.h"

void align_pci_memory(pci_alloc_t* allocator, uint64_t alignment, pci_space_t space);
uint64_t alloc_pci_memory(pci_alloc_t* allocator, uint64_t size, pci_space_t space);
uintptr_t pci_to_mem_addr(pci_range_t* ranges, uint64_t pci_addr, pci_space_t space);
uint64_t mem_to_pci_addr(pci_range_t* ranges, uintptr_t mem_addr, pci_space_t space);
void create_pci_device(pci_ctx_t* pci_ctx, pci_header0_t* header, uint64_t header_offset);

void discover_pcie(pci_ctx_t* pci_ctx, uint64_t header_count);
pci_range_t pcie_parse_ranges(dt_node_t* dt_node);
void* pcie_parse_allocate_reg(dt_node_t* dt_node, int64_t* size_out);
pci_interrupt_map_t* pcie_parse_interrupt_map(dt_node_t* dt_node);

