
#include <stdint.h>

#include "kernel/lib/libpci.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/interrupt/interrupt.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

pci_cap_t* pci_get_capability(pci_device_ctx_t* device_ctx, uint64_t cap_type, uint64_t idx) {
    pci_cap_t* cap;
    FOR_LLIST(device_ctx->cap_list, cap)
        if (cap->cap == cap_type) {
            if (idx == 0) {
                return cap;
            } else {
                idx--;
            }
        }
    END_FOR_LLIST()

    return NULL;
}

uint32_t pci_register_interrupt_handler(pci_device_ctx_t* device_ctx, pci_irq_handler_fn fn, void* ctx) {

    pci_msix_vector_ctx_t* msix_ctx = vmalloc(sizeof(pci_msix_vector_ctx_t));
    pci_cap_t* msix_cap = pci_get_capability(device_ctx, PCI_CAP_MSIX, 0);

    // Find an available msi-x vector
    uint64_t msix_entry_idx = 0;
    bool msix_alloc_ok;
    msix_alloc_ok = bitalloc_alloc_any(&device_ctx->msix_vector_alloc, &msix_entry_idx);
    ASSERT(msix_alloc_ok == true);

    pci_msix_table_entry_t* msix_entry = NULL;
    const uint32_t msix_cap_table_offset =
        PCI_READCAP32_DEV(device_ctx, 
                          msix_cap,
                          PCI_CAP_MSIX_TABLE_OFF);

    uint32_t msix_table_bar = msix_cap_table_offset & 0x7;
    ASSERT(device_ctx->bar[msix_table_bar].allocated);
    uint32_t msix_table_offset = msix_cap_table_offset & 0xFFFFFFF8;
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
    pci_cap_t* msix_cap = pci_get_capability(device_ctx, PCI_CAP_MSIX, 0);

    uint16_t msg_ctrl = PCI_READCAP16_DEV(device_ctx,
                                          msix_cap,
                                          PCI_CAP_MSIX_MSG_CTRL);

    PCI_WRITECAP16_DEV(device_ctx,
                       msix_cap,
                       PCI_CAP_MSIX_MSG_CTRL,
                       msg_ctrl | PCI_MSIX_CTRL_ENABLE);
    MEM_DSB();
}

void pci_disable_interrupts(pci_device_ctx_t* device_ctx) {
    pci_cap_t* msix_cap = pci_get_capability(device_ctx, PCI_CAP_MSIX, 0);

    uint16_t msg_ctrl = PCI_READCAP16_DEV(device_ctx,
                                          msix_cap,
                                          PCI_CAP_MSIX_MSG_CTRL);

    PCI_WRITECAP16_DEV(device_ctx,
                       msix_cap,
                       PCI_CAP_MSIX_MSG_CTRL,
                       msg_ctrl & ~(PCI_MSIX_CTRL_ENABLE));

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

    pci_cap_t* msix_cap = pci_get_capability(device_ctx, PCI_CAP_MSIX, 0);

    uint32_t msix_pba_off = PCI_READCAP32_DEV(device_ctx,
                                              msix_cap,
                                              PCI_CAP_MSIX_PBA_OFF);
    uint8_t pba_bar = msix_pba_off & 7;
    uint64_t pba_addr = msix_pba_off & (~7);
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
    uint8_t header_mem[4096];
    device_ctx->pci_ctx->header_ops.read(device_ctx->pci_ctx,
                                         PCI_DEV_ADDR(device_ctx, 0),
                                         header_mem);
    pci_header0_t* h0 = (pci_header0_t*)header_mem;
    console_printf("PCI Device\n");
    console_printf(" Addr %2x:%2x:%2x\n", device_ctx->bus, device_ctx->dev, device_ctx->func);
    console_printf(" Vendor %4x Device %4x Revision %u\n", h0->vendor_id, h0->device_id, h0->revision_id);
    console_printf(" Class %2x Subclass %2x Progif %2x\n", h0->class, h0->subclass, h0->prog_if);
    console_printf(" Subsystem %4x Subsystem Vendor %4x\n", h0->subsystem_id, h0->subsystem_vendor_id);
    console_printf(" Header %2x\n", h0->header_type);
    for (int i = 0; i < 6; i++) {
        console_printf(" BAR[%u] %x\n", i, h0->bar[i]);
    }
    console_printf(" Int Line/Pin %u/%u\n", h0->int_line, h0->int_pin);

    console_flush();
}

void pci_poke_bar_entry(pci_header0_t* header, uint32_t barnum, uint32_t barval) {
    header->bar[barnum] = barval;
    header->command = 7;
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

    pci_cap_t* cap;
    FOR_LLIST(device_ctx->cap_list, cap)
        
        console_printf("CAP[%2x]: %2x %s\n",
                       cap->offset,
                       cap->cap,
                       (cap->cap < 22) ? capability_names[cap->cap] : "Unknown");
        switch (cap->cap) {
            case PCI_CAP_MSIX:
                print_pci_capability_msix(device_ctx, cap);
                break;
            case PCI_CAP_VENDOR:
                print_pci_capability_vendor(device_ctx, cap);
                break;
            default:
                console_printf(" Unsupported\n");
                break;
        }

    END_FOR_LLIST()

    console_flush();
}

void print_pci_capability_msix(pci_device_ctx_t* device_ctx, pci_cap_t* cap) {

    console_printf(" MSI-X\n");
    console_printf("  Message Control %4x\n", PCI_READCAP16_DEV(device_ctx, cap, PCI_CAP_MSIX_MSG_CTRL));
    uint32_t table_offset = PCI_READCAP32_DEV(device_ctx, cap, PCI_CAP_MSIX_TABLE_OFF);
    console_printf("  Table Offset [%u] %8x\n",
                   table_offset & 0x7,
                   table_offset & 0xFFFFFFF8);
    uint32_t pba_offset = PCI_READCAP32_DEV(device_ctx, cap, PCI_CAP_MSIX_PBA_OFF);
    console_printf("  PBA Offset [%u] %8x\n",
                   pba_offset & 0x7,
                   pba_offset & 0xFFFFFFF8);
    console_flush();
}

void print_pci_capability_vendor(pci_device_ctx_t* device_ctx, pci_cap_t* cap) {

    (void)device_ctx;
    console_printf(" Vendor Specific\n");

    switch (device_ctx->vendor_id) {
        case PCI_VENDOR_QEMU:
            print_pci_capability_virtio(device_ctx, cap);
            break;
        default:
            console_printf("  Unknown Vendor\n");
            break;
    }

    console_flush();
}
