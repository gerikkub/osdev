
#include <stdint.h>

#include "kernel/lib/libpci.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/interrupt/interrupt.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

void pci_alloc_device_from_context(pci_device_ctx_t* device, discovery_pci_ctx_t* module_ctx) {
    ASSERT(device != NULL);
    ASSERT(module_ctx != NULL);

    // Allocate virtual memory space for the PCI header
    device->header_phy = module_ctx->header_phy;
    void* header_vmem = PHY_TO_KSPACE_PTR(device->header_phy);
    ASSERT(header_vmem != NULL);
    device->header_vmem = header_vmem;
    device->header_offset = module_ctx->header_offset;

    // Allocate virtual memory space for IO, M32 and M64 memories
    device->io_base_phy = module_ctx->io_base;
    device->io_size = module_ctx->io_size;
    device->io_base_vmem = NULL;
    if (module_ctx->io_size > 0) {
        void* pci_io_mem = PHY_TO_KSPACE_PTR(module_ctx->io_base);
        ASSERT(pci_io_mem);
        device->io_base_vmem = pci_io_mem;
    }

    device->m32_base_phy = module_ctx->m32_base;
    device->m32_size = module_ctx->m32_size;
    device->m32_base_vmem = NULL;
    if (module_ctx->m32_size > 0) {
        void* pci_m32_mem = PHY_TO_KSPACE_PTR(module_ctx->m32_base);
        ASSERT(pci_m32_mem);
        device->m32_base_vmem = pci_m32_mem;
    }

    device->m64_base_phy = module_ctx->m64_base;
    device->m64_size = module_ctx->m64_size;
    device->m64_base_vmem = NULL;
    if (module_ctx->m64_size > 0) {
        void* pci_m64_mem = PHY_TO_KSPACE_PTR(module_ctx->m64_base);
        ASSERT(pci_m64_mem);
        device->m64_base_vmem = pci_m64_mem;
    }

    for (int idx = 0; idx < MAX_PCI_BAR; idx++) {
        device->bar[idx].allocated = module_ctx->bar[idx].allocated;
        if (device->bar[idx].allocated) {
            device->bar[idx].phy = module_ctx->bar[idx].phy;
            device->bar[idx].space = module_ctx->bar[idx].space;
            device->bar[idx].len = module_ctx->bar[idx].len;
            uint64_t offset;
            switch (device->bar[idx].space) {
                case PCI_SPACE_IO:
                    offset = device->bar[idx].phy - device->io_base_phy;
                    device->bar[idx].vmem = device->io_base_vmem + offset;
                    break;
                case PCI_SPACE_M32:
                    offset = device->bar[idx].phy - device->m32_base_phy;
                    device->bar[idx].vmem = device->m32_base_vmem + offset;
                    break;
                case PCI_SPACE_M64:
                    offset = device->bar[idx].phy - device->m64_base_phy;
                    device->bar[idx].vmem = device->m64_base_vmem + offset;
                    break;
                default:
                    ASSERT(0);
                    break;
            }
        }
    }

    device->msix_vector_list = llist_create();
    device->msix_cap = (pci_msix_capability_t*)pci_get_capability(device, PCI_CAP_MSIX, 0);
    ASSERT(device->msix_cap != NULL);

    uint32_t num_msix_entries = (device->msix_cap->msg_ctrl & PCI_MSIX_CTRL_SIZE_MASK) + 1;

    bitalloc_init(&device->msix_vector_alloc, 0, num_msix_entries, vmalloc);
}

pci_generic_capability_t* pci_get_capability(pci_device_ctx_t* device_ctx, uint64_t cap, uint64_t idx) {

    pci_header0_t* h = device_ctx->header_vmem;
    uint8_t* cap_offset_ptr = &h->capabilities_ptr;
    pci_generic_capability_t* cap_ptr;

    while (*cap_offset_ptr != 0) {
        cap_ptr = device_ctx->header_vmem + *cap_offset_ptr;

        if (cap_ptr->cap == cap) {
            if (idx == 0) {
                break;
            } else {
                idx--;
            }
        }

        cap_offset_ptr = &cap_ptr->next;
    }

    if (*cap_offset_ptr != 0) {
        return cap_ptr;
    } else {
        return NULL;
    }
}

uint32_t pci_register_interrupt_handler(pci_device_ctx_t* device_ctx, pci_irq_handler_fn fn, void* ctx) {

    pci_msix_vector_ctx_t* msix_ctx = vmalloc(sizeof(pci_msix_vector_ctx_t));

    // Find an available msi-x vector
    uint64_t msix_entry_idx = 0;
    bool msix_alloc_ok;
    msix_alloc_ok = bitalloc_alloc_any(&device_ctx->msix_vector_alloc, &msix_entry_idx);
    ASSERT(msix_alloc_ok == true);

    pci_msix_table_entry_t* msix_entry = NULL;
    uint32_t msix_table_bar = device_ctx->msix_cap->table_offset & 0x7;
    ASSERT(device_ctx->bar[msix_table_bar].allocated);
    uint32_t msix_table_offset = device_ctx->msix_cap->table_offset & 0xFFFFFFF8;
    msix_entry = device_ctx->bar[msix_table_bar].vmem + msix_table_offset +
                 (sizeof(pci_msix_table_entry_t)*msix_entry_idx);

    // Allocate an available SPI interrupt
    uint64_t intid;
    uint64_t msix_data;
    uintptr_t msix_ptr;
    interrupt_get_msi(&intid, &msix_data, &msix_ptr);
    interrupt_register_irq_handler(intid, fn, ctx);

    // Populate the msi-x table
    msix_entry->msg_addr_lower = msix_ptr & 0xFFFFFFFF;
    msix_entry->msg_addr_upper = (msix_ptr >> 32UL) & 0xFFFFFFFF;
    msix_entry->msg_data = msix_data & 0xFFFFFFFF;

    msix_ctx->entry = msix_entry;
    msix_ctx->entry_idx = msix_entry_idx;
    msix_ctx->intid = intid;
    msix_ctx->handler = fn;
    msix_ctx->ctx = ctx;

    llist_append_ptr(device_ctx->msix_vector_list, msix_ctx);

    return intid;
}

void pci_enable_interrupts(pci_device_ctx_t* device_ctx) {
    device_ctx->msix_cap->msg_ctrl |= PCI_MSIX_CTRL_ENABLE;
    MEM_DSB();
}

void pci_disable_interrupts(pci_device_ctx_t* device_ctx) {
    device_ctx->msix_cap->msg_ctrl &= ~(PCI_MSIX_CTRL_ENABLE);
    MEM_DSB();
}

void pci_enable_vector(pci_device_ctx_t* device_ctx, uint32_t intid) {

    pci_msix_vector_ctx_t* item = NULL;
    pci_msix_vector_ctx_t* founditem = NULL;

    FOR_LLIST(device_ctx->msix_vector_list, item)
        if (item->intid == intid) {
            founditem = item;
            break;
        }
    END_FOR_LLIST()

    ASSERT(founditem != NULL);
    founditem->entry->vector_ctrl &= ~(PCI_MSIX_VECTOR_MASKED);

    interrupt_enable_irq(intid);
}

void pci_disable_vector(pci_device_ctx_t* device_ctx, uint32_t intid) {

    interrupt_disable_irq(intid);

    pci_msix_vector_ctx_t* item = NULL;
    pci_msix_vector_ctx_t* founditem = NULL;

    FOR_LLIST(device_ctx->msix_vector_list, item)
        if (item->intid == intid) {
            founditem = item;
            break;
        }
    END_FOR_LLIST()

    ASSERT(founditem != NULL);
    founditem->entry->vector_ctrl |= PCI_MSIX_VECTOR_MASKED;
}

void pci_interrupt_clear_pending(pci_device_ctx_t* device_ctx, uint32_t intid) {

    pci_msix_vector_ctx_t* item = NULL;
    pci_msix_vector_ctx_t* founditem = NULL;

    FOR_LLIST(device_ctx->msix_vector_list, item)
        if (item->intid == intid) {
            founditem = item;
            break;
        }
    END_FOR_LLIST()

    ASSERT(founditem != NULL);

    uint8_t pba_bar = device_ctx->msix_cap->pba_offset & 7;
    uint64_t pba_addr = device_ctx->msix_cap->pba_offset & (~7);
    ASSERT(device_ctx->bar[pba_bar].allocated);
    uint64_t* pend_table = device_ctx->bar[pba_bar].vmem + pba_addr;

    uint64_t pend_word = founditem->entry_idx / 64;
    uint64_t pend_bit = founditem->entry_idx % 64;

    pend_table[pend_word] &= ~(pend_bit);

    MEM_DMB();
}

pci_msix_vector_ctx_t* pci_get_msix_entry(pci_device_ctx_t* device_ctx, uint32_t intid) {

    pci_msix_vector_ctx_t* msix_item = NULL;

    FOR_LLIST(device_ctx->msix_vector_list, msix_item)
        if (msix_item->intid == intid) {
            return msix_item;
        }
    END_FOR_LLIST()

    return NULL;
}

void print_pci_header(pci_device_ctx_t* device_ctx) {
    pci_header0_t* h0 = device_ctx->header_vmem;
    console_printf("PCI Device\n");
    console_printf(" Offset %8x\n", device_ctx->header_offset);
    console_printf(" Vendor %4x Device %4x Revision %u\n", h0->vendor_id, h0->device_id, h0->revision_id);
    console_printf(" Class %4x Subclass %4x\n", h0->class, h0->subclass);
    console_printf(" Subsystem %4x Subsystem Vendor %4x\n", h0->subsystem_id, h0->subsystem_vendor_id);
    console_printf(" Header %2x\n", h0->header_type);
    for (int i = 0; i < 6; i++) {
        console_printf(" BAR[%u] %x\n", i, h0->bar[i]);
    }
    console_printf(" Int Line/Pin %u/%u\n", h0->int_line, h0->int_pin);

    console_flush();
}

const char* capability_names[] = {
    "Null",
    "PCI Power Management Inferface",
    "AGP",
    "VPD",
    "Slot Identification",
    "MSI",
    "CompactPCI Hot Swap",
    "PCI-X",
    "HyperTransport",
    "Vendor Specific",
    "Debug",
    "Compact PCI Centra Resource Control",
    "PCI Hot-Plug",
    "PCI Bridge Subsystem Vendor ID",
    "AGP 8x",
    "Secure Device",
    "PCI Express",
    "MSI-X",
    "Serial ATA",
    "Advanced Features",
    "Enhanced Allocation",
    "Flattening Portal Bridge"
};

void print_pci_capabilities(pci_device_ctx_t* device_ctx) {

    pci_header0_t* h = device_ctx->header_vmem;
    uint8_t* cap_offset_ptr = &h->capabilities_ptr;
    pci_generic_capability_t* capability;

    while (*cap_offset_ptr != 0) {
        capability = device_ctx->header_vmem + *cap_offset_ptr;
        console_printf("CAP[%2x]: %2x\n",
                       *cap_offset_ptr,
                       capability->cap);

        switch (capability->cap) {
            case PCI_CAP_MSIX:
                print_pci_capability_msix(device_ctx, (pci_msix_capability_t*)capability);
                break;
            case PCI_CAP_VENDOR:
                print_pci_capability_vendor(device_ctx, (pci_vendor_capability_t*)capability);
                break;
            default:
                console_printf(" Unsupported\n");
                break;
        }

        cap_offset_ptr = &capability->next;
    }
    console_flush();
}

void print_pci_capability_msix(pci_device_ctx_t* device_ctx, pci_msix_capability_t* cap_ptr) {

    console_printf(" MSI-X\n");
    console_printf("  Message Control %4x\n",
                   cap_ptr->msg_ctrl);
    console_printf("  Table Offset [%u] %8x\n",
                   cap_ptr->table_offset & 0x7,
                   cap_ptr->table_offset & 0xFFFFFFF8);
    console_printf("  PBA Offset [%u] %8x\n",
                   cap_ptr->pba_offset & 0x7,
                   cap_ptr->pba_offset & 0xFFFFFFF8);
    console_flush();
}

void print_pci_capability_vendor(pci_device_ctx_t* device_ctx, pci_vendor_capability_t* cap_ptr) {

    (void)device_ctx;
    (void)cap_ptr;
    console_printf(" Vendor Specific\n");

    pci_header0_t* header = device_ctx->header_vmem;

    switch (header->vendor_id) {
        case PCI_VENDOR_QEMU:
            print_pci_capability_qemu(device_ctx, cap_ptr);
            break;
        default:
            console_printf("  Unknown Vendor\n");
            break;
    }

    console_flush();
}