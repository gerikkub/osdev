
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/libdtb.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/task.h"
#include "kernel/kmalloc.h"
#include "kernel/gtimer.h"
#include "kernel/interrupt/interrupt.h"

#include "drivers/spi/bcm2835_spi/bcm2835_spi.h"
#include "drivers/spi/spi.h"

#include "include/k_ioctl_common.h"
#include "include/k_spi.h"

#include "kernel/lib/utils.h"
#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

typedef struct {
    dt_node_t* dt_node;

    void* mem;

    uint64_t osc_freq;
    uint64_t intid;

    spi_txn_t* active_txn;
} bcm2835_spi_ctx_t;

static void bcm2835_spi_irq_handler(uint32_t intid, void* ctx) {
    bcm2835_spi_ctx_t* spi_ctx = ctx;
    uint32_t* gpio_set = (uint32_t*)(0xffff00047e200000 + 0x1c);
    uint32_t* gpio_clr = (uint32_t*)(0xffff00047e200000 + 0x28);
    *gpio_clr = (1 << 3);


    uint32_t status = READ_MEM32(spi_ctx->mem + BCM2711_SPI_CS);

    while (status & BCM2711_SPI_CS_RXD &&
        spi_ctx->active_txn->read_pos < spi_ctx->active_txn->len) {

        uint32_t r = READ_MEM32(spi_ctx->mem + BCM2711_SPI_FIFO);
        spi_ctx->active_txn->read_mem[spi_ctx->active_txn->read_pos] = r;
        spi_ctx->active_txn->read_pos++;

        status = READ_MEM32(spi_ctx->mem + BCM2711_SPI_CS);
    }

    status = READ_MEM32(spi_ctx->mem + BCM2711_SPI_CS);
    while (status & BCM2711_SPI_CS_TXD &&
        spi_ctx->active_txn->write_pos < spi_ctx->active_txn->len) {

        uint32_t w = spi_ctx->active_txn->write_mem[spi_ctx->active_txn->write_pos];
        WRITE_MEM32(spi_ctx->mem + BCM2711_SPI_FIFO, w);
        spi_ctx->active_txn->write_pos++;

        status = READ_MEM32(spi_ctx->mem + BCM2711_SPI_CS);
    }

    if (status & BCM2711_SPI_CS_DONE &&
        (spi_ctx->active_txn->read_pos == spi_ctx->active_txn->len) &&
        (spi_ctx->active_txn->write_pos == spi_ctx->active_txn->len)) {

        status &= ~(BCM2711_SPI_CS_TA);
        WRITE_MEM32(spi_ctx->mem + BCM2711_SPI_CS, status);

        spi_ctx->active_txn->complete = true;
    }

    *gpio_set = (1 << 3);
}

static int64_t bcm2835_spi_execute_txn(void* ctx, k_spi_device_t* device_cfg, spi_txn_t* txn) {
    bcm2835_spi_ctx_t* spi_ctx = ctx;

    uint32_t baud_divisor = spi_ctx->osc_freq / device_cfg->clk_hz;
    uint32_t spi_config = BCM2711_SPI_CS_INTR |
                          BCM2711_SPI_CS_INTD |
                          BCM2711_SPI_CS_TA |
                          ((device_cfg->flags & SPI_DEVICE_FLAG_CS_AH) ?
                          BCM2711_SPI_CS_CSPOL : 0) |
                          BCM2711_SPI_CS_CLEAR_RX |
                          BCM2711_SPI_CS_CLEAR_TX |
                          ((device_cfg->flags & SPI_DEVICE_FLAG_CPOL_HIGH) ?
                          BCM2711_SPI_CS_CPOL : 0) |
                          ((device_cfg->flags & SPI_DEVICE_FLAG_CPHA_BEGIN) ?
                          BCM2711_SPI_CS_CPHA : 0) |
                          (device_cfg->flags & SPI_DEVICE_FLAG_CS_MASK & 3);

    spi_ctx->active_txn = txn;

    WRITE_MEM32(spi_ctx->mem + BCM2711_SPI_CLK, baud_divisor);
    WRITE_MEM32(spi_ctx->mem + BCM2711_SPI_CS, spi_config);

    while (true) {
        interrupt_await_reset(spi_ctx->intid);
        if (spi_ctx->active_txn->complete) {
            break;
        }
        interrupt_await(spi_ctx->intid);
    }

    spi_txn_complete(spi_ctx->active_txn);
    spi_ctx->active_txn = NULL;

    return txn->len;
}

static spi_ops_t s_spi_ops = {
    .execute_fn = bcm2835_spi_execute_txn
};

static int64_t bcm2835_spi_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    *ctx_out = ctx;
    return 0;
}

static int64_t bcm2835_spi_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {

    switch (ioctl) {
        case SPI_IOCTL_CREATE_DEVICE:
            if (arg_count != 1) {
                return -1;
            }
            return spi_create_device(ctx, &s_spi_ops, (k_spi_device_t*)args[0]);
        default:
            return -1;
    }
}

static int64_t bcm2835_spi_close_op(void* ctx) {
    return 0;
}

static fd_ops_t s_spi_file_ops = {
    .read = NULL,
    .write = NULL,
    .ioctl = bcm2835_spi_ioctl_op,
    .close = bcm2835_spi_close_op,
};

void bcm2835_spi_irq_reg(void* ctx) {
    bcm2835_spi_ctx_t* spi_ctx = ctx;

    ASSERT(spi_ctx->dt_node->prop_ints);
    ASSERT(spi_ctx->dt_node->prop_ints->num_ints == 1);
    ASSERT(spi_ctx->dt_node->prop_ints->handler);
    ASSERT(spi_ctx->dt_node->prop_ints->handler->dtb_funcs);

    dt_node_t* intc_node = spi_ctx->dt_node->prop_ints->handler;
    intc_dtb_funcs_t* intc_dtb_funcs = intc_node->dtb_funcs;

    intc_dtb_funcs->get_intid_list(intc_node->dtb_ctx,
                                   spi_ctx->dt_node->prop_ints,
                                   &spi_ctx->intid);

    intc_dtb_funcs->setup_intids(intc_node->dtb_ctx,
                                 spi_ctx->dt_node->prop_ints);

    interrupt_register_irq_handler(spi_ctx->intid, bcm2835_spi_irq_handler, ctx);
    interrupt_enable_irq(spi_ctx->intid);
}

void bcm2835_spi_discover(void* ctx) {
    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    bcm2835_spi_ctx_t* spi_ctx = vmalloc(sizeof(bcm2835_spi_ctx_t));
    spi_ctx->dt_node = dt_node;

    dt_prop_reg_entry_t* reg_entries = dt_node->prop_reg->reg_entries;

    ASSERT(reg_entries->addr_size == 1);
    uintptr_t addr_bus = reg_entries->addr_ptr[0];

    bool addr_valid;
    uintptr_t spi_ctx_phy = dt_map_addr_to_phy(dt_node, addr_bus, &addr_valid);
    ASSERT(addr_valid);

    // TODO: Parse devicetree aliases
    if ((spi_ctx_phy & 0xFFFFFFFF) == 0x7e204000) {
        uint64_t phy_page_aligned = spi_ctx_phy & ~(VMEM_PAGE_SIZE-1);
        // TODO: This function seems to be breaking when initializing all SPIs
        memspace_map_device_kernel((void*)phy_page_aligned,
                                    PHY_TO_KSPACE_PTR(phy_page_aligned),
                                    VMEM_PAGE_SIZE,
                                    MEMSPACE_FLAG_PERM_KRW |
                                    MEMSPACE_FLAG_IGNORE_DUPS);
        memspace_update_kernel_vmem();

        spi_ctx->mem = PHY_TO_KSPACE_PTR(spi_ctx_phy);
        // TODO: Pull this out of device tree. It can also be dynamic
        // on the Raspi 4B...
        spi_ctx->osc_freq = 500000000;

        console_log(LOG_INFO, "BCM2835 SPI @ %8x", spi_ctx_phy);
        sys_device_register(&s_spi_file_ops, bcm2835_spi_open_op, spi_ctx, "spi0");

        driver_register_late_init(bcm2835_spi_irq_reg, spi_ctx);
    }
}

void bcm2835_spi_register() {

    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "brcm,bcm2835-spi"
        },
        .ctxfunc = bcm2835_spi_discover
    };
    register_driver(&reg);
}

REGISTER_DRIVER(bcm2835_spi);