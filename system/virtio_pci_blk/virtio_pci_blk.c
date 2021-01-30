
#include <stdint.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_console.h"
#include "system/lib/libpci.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

static pci_device_ctx_t s_pci_device = {0};

static void init_blk_device(pci_device_ctx_t* pci_ctx) {

    pci_header0_t* pci_header = pci_ctx->header_vmem;
    pci_header->command |= 3;

    print_pci_header(pci_ctx);

    print_pci_capabilities(pci_ctx);

    for (int idx = 0; idx < 6; idx++) {
        if (pci_ctx->bar[idx].allocated) {
            uint32_t* vmem = pci_ctx->bar[idx].vmem;
            uint64_t len = pci_ctx->bar[idx].len / 4;
            switch (pci_ctx->bar[idx].space) {
                case PCI_SPACE_IO:
                    console_printf("Bar [%u] IO Len %x\n", idx, len/4);
                    break;
                case PCI_SPACE_M32:
                    console_printf("Bar [%u] M32 Len %x\n", idx, len * 4);
                    break;
                case PCI_SPACE_M64:
                    console_printf("Bar [%u] M64 Len %x\n", idx, len * 4);
                    break;
                default:
                    SYS_ASSERT(0);
            }
            console_flush();

            (void)vmem;
/*
            int offset = 0;
            while (offset + 4 <= len) {
                console_printf("%4x: %8x %8x %8x %8x\n", offset*4,
                                vmem[offset], vmem[offset+1],
                                vmem[offset+2], vmem[offset+3]);
                console_flush();
                offset += 4;
            }
            if (offset != len) {
                console_printf("%4x:", offset*4);
                while (offset != len) {
                    console_printf(" %8x", vmem[offset]);
                    offset++;
                }
            }

            console_flush();
            */
        }
    }
}

static void virtio_pci_blk_ctx(system_msg_memory_t* ctx_msg) {

    console_write("virtio-pci-blk got ctx\n");
    console_flush();

    module_pci_ctx_t* pci_ctx = (module_pci_ctx_t*)ctx_msg->ptr;

    pci_alloc_device_from_context(&s_pci_device, pci_ctx);

    init_blk_device(&s_pci_device);
}

static module_handlers_t s_handlers = {
    { //generic
        .info = NULL,
        .getinfo = NULL,
        .ioctl = NULL,
        .ctx = virtio_pci_blk_ctx
    }
};

void main(uint64_t my_tid, module_ctx_t* ctx) {

    system_register_handler(s_handlers, MOD_CLASS_DISCOVERY);

    console_write("virtio-pci-blk driver\n");
    console_flush();

    while (1) {
        system_recv_msg();
    }
}