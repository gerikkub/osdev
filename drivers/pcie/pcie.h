
#pragma once

#include <stdint.h>

#include "kernel/lib/libpci.h"
#include "kernel/lib/libdtb.h"

void align_pci_memory(pci_alloc_t* allocator, uint64_t alignment, pci_space_t space);
uint64_t alloc_pci_memory(pci_alloc_t* allocator, uint64_t size, pci_space_t space);
uintptr_t pci_to_mem_addr(pci_range_t* ranges, uint64_t pci_addr, pci_space_t space);
uint64_t mem_to_pci_addr(pci_range_t* ranges, uintptr_t mem_addr, pci_space_t space);
pci_device_ctx_t* create_pci_device(pci_ctx_t* pci_ctx, uint8_t bus, uint8_t dev, uint8_t func);

void pcie_apply_bridge_bus_config(pci_ctx_t* pci_ctx, pci_bridge_tree_t* node);
void scan_pcie_topology(pci_ctx_t* pci_ctx);
void pcie_fixup_bridge_tree(pci_ctx_t* pci_ctx);
void discover_pcie(pci_ctx_t* pci_ctx);
pci_range_t pcie_parse_ranges(dt_node_t* dt_node);
void* pcie_parse_allocate_reg(dt_node_t* dt_node, int64_t* size_out);
pci_interrupt_map_t* pcie_parse_interrupt_map(dt_node_t* dt_node);

