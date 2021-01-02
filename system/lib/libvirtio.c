
#include <stdint.h>

#include "system/lib/libpci.h"
#include "system/lib/libvirtio.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_lib.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

void print_pci_capability_qemu(pci_device_ctx_t* device_ctx, pci_vendor_capability_t* cap_ptr) {

    const char* qemu_type_names[] = {
        "Unknown",
        "Virtio PCI Cap Common Cfg",
        "Virtio PCI Cap Notify Cfg",
        "Virito PCI Cap ISR Cfg",
        "Virito PCI Cap Device Cfg",
        "Virito PCI Cap PCI Cfg",
    };

    pci_qemu_capability_t* qemu_cap_ptr = (pci_qemu_capability_t*) cap_ptr;

    console_printf("  QEMU\n");
    if (qemu_cap_ptr->type < VIRTIO_PCI_CAP_MAX) {
        console_printf("  %s\n", qemu_type_names[qemu_cap_ptr->type]);
        console_printf("  BAR [%u] %8x %8x\n",
                       qemu_cap_ptr->bar,
                       qemu_cap_ptr->bar_offset,
                       qemu_cap_ptr->bar_len);

        switch (qemu_cap_ptr->type) {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                print_qemu_capability_common(device_ctx, qemu_cap_ptr);
                break;
        }
    } else {
        console_printf("  Invalid Type %u\n", qemu_cap_ptr->type);
    }

    console_flush();
}

void print_qemu_capability_common(pci_device_ctx_t* device_ctx, pci_qemu_capability_t* cap_ptr) {

    pci_qemu_virtio_common_cfg_t* common_cfg;

    common_cfg = device_ctx->bar[cap_ptr->bar].vmem + cap_ptr->bar_offset;
    common_cfg->device_feature_sel = 1;
    console_printf("   Device Features: %8x", common_cfg->device_feature);
    common_cfg->device_feature_sel = 0;
    console_printf(" %8x\n", common_cfg->device_feature);

    common_cfg->driver_feature_sel = 1;
    console_printf("   Driver Features: %8x", common_cfg->driver_feature);
    common_cfg->driver_feature_sel = 0;
    console_printf(" %8x\n", common_cfg->driver_feature);

    console_printf("   MSI-X Vector: %u\n", common_cfg->msix_config);
    console_printf("   Num Queues: %u\n", common_cfg->num_queues);
    console_printf("   Device Status: %2x\n", common_cfg->device_status);

    console_flush();

    for (int idx = 0; idx < common_cfg->num_queues; idx++) {
        common_cfg->queue_select = idx;
        console_printf("   Queue %u\n", idx);
        console_printf("    Size: %x\n", common_cfg->queue_size);
        console_printf("    MSI-X vector: %x\n", common_cfg->queue_msix_vector);
        console_printf("    Enable: %x\n", common_cfg->queue_enable);
        console_printf("    Notify Offset: %x\n", common_cfg->queue_notify_off);
        console_printf("    Desc: %x\n", common_cfg->queue_desc);
        console_printf("    Driver: %x\n", common_cfg->queue_driver);
        console_printf("    Device: %x\n", common_cfg->queue_device);
        console_flush();
    }

    console_flush();
}
