

#include <stdint.h>

#include "kernel/lib/vmalloc.h"

#include "stdlib/bitutils.h"
#include "stdlib/bitalloc.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/kernelspace.h"
#include "kernel/drivers.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/lib/libdtb.h"

#include "drivers/gicv3/gicv3.h"

#define NUM_FAKE_MSI_PORTS 64

typedef struct {
    bitalloc_t intid_alloc;
    volatile GICD_Struct* gicd;
    volatile GICR_Struct* gicr;
    volatile GICR_PPI_Struct* gicrppi;
} gicv3_ctx_t;

static gicv3_ctx_t s_gicv3_ctx;

static uint32_t s_gicv3_msi_words[NUM_FAKE_MSI_PORTS];

void gicv3_enable(void* ctx) {

    uint64_t icc_ctlr = 0;
    WRITE_SYS_REG(ICC_REG_CTLR_EL1, icc_ctlr);

    uint64_t icc_igrpen1 = ICC_IGRPEN1_ENABLE;
    WRITE_SYS_REG(ICC_REG_IGRPEN1_EL1, icc_igrpen1);

    // Enable the Distributor
    s_gicv3_ctx.gicd->ctlr = GICD_CTRL_ENG1;

    uint64_t icc_pmr = 0xFF;
    WRITE_SYS_REG(ICC_REG_PMR_EL1, icc_pmr);

    uint64_t icc_bpr1 = 0;
    WRITE_SYS_REG(ICC_REG_BPR1_EL1, icc_bpr1);

}

void gicv3_disable(void* ctx) {
    uint64_t icc_igrpen1 = 0;
    WRITE_SYS_REG(ICC_REG_IGRPEN1_EL1, icc_igrpen1);
    s_gicv3_ctx.gicd->ctlr &= ~(GICD_CTRL_ENG1);
}

void gicv3_enable_intid(void* ctx, uint64_t intid) {

    ASSERT(intid < GIC_MAX_IRQS);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    if (intid_word == 0) {
        s_gicv3_ctx.gicrppi->igroupr0 |= BIT(intid_bit);
        s_gicv3_ctx.gicrppi->isenabler0 = BIT(intid_bit);
    } else {
        s_gicv3_ctx.gicd->igroupr[intid_word] |= BIT(intid_bit);
        s_gicv3_ctx.gicd->isenabler[intid_word] = BIT(intid_bit);
    }
}

void gicv3_disable_intid(void* ctx, uint64_t intid) {

    ASSERT(intid < GIC_MAX_IRQS);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    if (intid_word == 0) {
        s_gicv3_ctx.gicrppi->icenabler0 = BIT(intid_bit);
    } else {
        s_gicv3_ctx.gicd->icenabler[intid_word] = BIT(intid_bit);
    }

}

void gicv3_get_spi_msi_intid(void* ctx, uint64_t* intid_out, uint64_t* data_out, uintptr_t* addr_out) {

    uint64_t intid;
    bool ok;
    ok = bitalloc_alloc_any(&s_gicv3_ctx.intid_alloc, &intid);
    ASSERT(ok);

    // GICD emulated MSI
    *intid_out = intid;
    *data_out = intid;
    *addr_out = KSPACE_TO_PHY(&s_gicv3_ctx.gicd->setspi_nsr);

    s_gicv3_ctx.gicd->icfgr[intid / 16] = (0x2 << ((intid % 16) * 2));
    // Set Indit to edge triggered

    console_log(LOG_DEBUG, "GICv3: Allocated SPI %d", intid);
}

void gicv3_set_spi_trigger(void* ctx, uint64_t intid, interrupt_trigger_type_t type) {
    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    if (type  == INTERRUPT_TRIGGER_LEVEL) {
        s_gicv3_ctx.gicd->icfgr[intid_word] &= ~(intid_bit);
    } else {
        s_gicv3_ctx.gicd->icfgr[intid_word] |= intid_bit;
    }
}

static intc_funcs_t intc_funcs = {
    .enable = gicv3_enable,
    .disable = gicv3_disable,
    .enable_irq = gicv3_enable_intid,
    .disable_irq = gicv3_disable_intid,
    .set_irq_trigger = gicv3_set_spi_trigger,
    .get_msi = gicv3_get_spi_msi_intid
};

bool gic_try_irq_handler(void) {
    uint32_t intid = 0;
    READ_SYS_REG(ICC_REG_IAR1_EL1, intid);

    if (intid == 1023) {
        return false;
    }

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    s_gicv3_ctx.gicd->icpendr[intid_word] = BIT(intid_bit);

    interrupt_handle_irq(intid);

    WRITE_SYS_REG(ICC_REG_EOIR1_EL1, intid);

    return true;
}

void gic_irq_handler(uint32_t vector) {

    uint32_t intid = 0;
    READ_SYS_REG(ICC_REG_IAR1_EL1, intid);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    s_gicv3_ctx.gicd->icpendr[intid_word] = BIT(intid_bit);

    interrupt_handle_irq(intid);

    WRITE_SYS_REG(ICC_REG_EOIR1_EL1, intid);
}

void gicv3_discover(void* ctx) {
    dt_block_t* dt_block = ((discovery_dtb_ctx_t*)ctx)->block;

    dt_node_t* dt_node = (dt_node_t*)&dt_block->data[dt_block->node_off];

    ASSERT(dt_node->std_properties_mask & DT_PROP_REG);

    // TODO: Need to figure out how to properly allocate this
    // uint64_t dt_num_reg = dt_node->reg.num_regs;
    // ASSERT(dt_num_reg >= DTB_REG_IDX_Max);
    uintptr_t* gic_reg_phy = (uint64_t*)&dt_block->data[dt_node->reg.reg_entries_off];
    s_gicv3_ctx.gicd = PHY_TO_KSPACE_PTR(gic_reg_phy[DTB_REG_IDX_GICD]);
    s_gicv3_ctx.gicr = PHY_TO_KSPACE_PTR(gic_reg_phy[DTB_REG_IDX_GICR]);
    s_gicv3_ctx.gicrppi = PHY_TO_KSPACE_PTR(gic_reg_phy[DTB_REG_IDX_GICR] + 64*1024);

    memspace_map_device_kernel((void*)gic_reg_phy[DTB_REG_IDX_GICD],
                               PHY_TO_KSPACE_PTR(gic_reg_phy[DTB_REG_IDX_GICD]),
                               gic_reg_phy[DTB_REG_IDX_GICD + 1],
                               MEMSPACE_FLAG_PERM_KRW);
    memspace_map_device_kernel((void*)gic_reg_phy[DTB_REG_IDX_GICR],
                               PHY_TO_KSPACE_PTR(gic_reg_phy[DTB_REG_IDX_GICR]),
                               gic_reg_phy[DTB_REG_IDX_GICR + 1],
                               MEMSPACE_FLAG_PERM_KRW);
    memspace_update_kernel_vmem();

    console_log(LOG_DEBUG, "Discovered GIC");
    console_log(LOG_DEBUG, " GICD_CTLR: %8x", s_gicv3_ctx.gicd->ctlr);
    console_log(LOG_DEBUG, " GICD_TYPER: %8x", s_gicv3_ctx.gicd->typer);
    console_log(LOG_DEBUG, " GICD_CTLR: %8x", s_gicv3_ctx.gicr->ctlr);
    console_log(LOG_DEBUG, " GICD_TYPER: %8x", s_gicv3_ctx.gicr->typer);

    bitalloc_init(&s_gicv3_ctx.intid_alloc, GIC_INTID_SPI_BASE, GIC_INTID_SPI_LIMIT, vmalloc);
    //bitalloc_init(&s_gicv3_ctx.intid_alloc, GIC_INTID_SPI_BASE + 500, GIC_INTID_SPI_LIMIT, vmalloc);

    for (uint64_t idx = 0; idx < NUM_FAKE_MSI_PORTS; idx++) {
        s_gicv3_msi_words[idx] = 0;
    }

    interrupt_register_controller(&intc_funcs, NULL, GIC_INTID_SPI_LIMIT);
}

void gicv3_poll_msi(void) {
    for (uint64_t idx = 0; idx < NUM_FAKE_MSI_PORTS; idx++) {
        volatile uint32_t* msi_word = &s_gicv3_msi_words[idx];
        if (*msi_word != 0) {
            //console_printf("Handle MSI IRQ %d\n", idx + GIC_INTID_SPI_BASE + 500);
            interrupt_handle_irq(idx + GIC_INTID_SPI_BASE + 500);
            *msi_word = 0;
        }
    }
}

void gicv3_register(void) {
    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "arm,gic-v3"
        },
        .ctxfunc = gicv3_discover
    };
    register_driver(&reg);

}

REGISTER_DRIVER(gicv3)