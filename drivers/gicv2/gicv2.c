

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

#include "drivers/gicv2/gicv2.h"

#include "stdlib/bitutils.h"

typedef struct {
    bitalloc_t intid_alloc;
    volatile GICDv2_Struct* gicd;
    volatile GICCv2_Struct* gicc;

    uint64_t max_intid;

    bool must_deactivate;
} gicv2_ctx_t;

void gicv2_enable(void* ctx) {
    gicv2_ctx_t* gic = ctx;

    // Set CPU Priority Mask to 128
    gic->gicc->pmr = 0xF0;

    // Set binary point to minimum value
    gic->gicc->bpr = 1;

    // Enable the Controller and Distributor
    gic->gicd->ctlr = GICDv2_CTLR_ENABLE;
    gic->gicc->ctlr |= GICCv2_CTLR_ENABLEGRP1;

    console_log(LOG_INFO, "GICv2 Enabled");
}

void gicv2_disable(void* ctx) {
    gicv2_ctx_t* gic = ctx;
    gic->gicd->ctlr = 0;
    gic->gicc->ctlr = 0;
}

void gicv2_enable_intid(void* ctx, uint64_t intid) {
    gicv2_ctx_t* gic = ctx;

    ASSERT(intid < gic->max_intid);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    gic->gicd->igroupr[intid_word] |= BIT(intid_bit);
    gic->gicd->isenabler[intid_word] = BIT(intid_bit);

    gic->gicd->ipriorityr[intid / 4] &= ~(0xFF << ((intid % 4)*8));
    gic->gicd->itargetsr[intid / 4] |= (0x1 << ((intid % 4)*8));

    // gic->gicd->icfgr[intid / 16] = 0;

    console_log(LOG_DEBUG, "Enable INTID %d", intid);
}

void gicv2_disable_intid(void* ctx, uint64_t intid) {
    gicv2_ctx_t* gic = ctx;

    ASSERT(intid < gic->max_intid);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    gic->gicd->icenabler[intid_word] = BIT(intid_bit);

    console_log(LOG_DEBUG, "Disable INTID %d", intid);
}

void gicv2_set_spi_trigger(void* ctx, uint64_t intid, interrupt_trigger_type_t type) {
    gicv2_ctx_t* gic = ctx;

    uint32_t intid_word = intid / 16;
    uint32_t intid_bit = intid % 16;

    gicv2_disable_intid(ctx, intid);

    if (type  == INTERRUPT_TRIGGER_LEVEL) {
        gic->gicd->icfgr[intid_word] &= ~(0x2 << (intid_bit*2));
    } else {
        gic->gicd->icfgr[intid_word] |= (0x2 << (intid_bit*2));
    }
}

static intc_funcs_t intc_funcs = {
    .enable = gicv2_enable,
    .disable = gicv2_disable,
    .enable_irq = gicv2_enable_intid,
    .disable_irq = gicv2_disable_intid,
    .set_irq_trigger = gicv2_set_spi_trigger,
    .get_msi = NULL,
    .irq_handler = gicv2_irq_handler
};

static void gicv2_dtb_get_intid_list(void* ctx, dt_prop_ints_t* ints_prop, uint64_t* intids) {
    ASSERT(intids != NULL);

    for (uint64_t idx = 0; idx < ints_prop->num_ints; idx++) {
        uint64_t intid = 0;
        if (ints_prop->int_entries[idx].data[0] == 0) {
            // SPI interrupt. Add SPI offset
            intid = GICv2_INTID_SPI_BASE;
        } else {
            // PPI interrupt. Add PPi offset
            intid = GICv2_INTID_PPI_BASE;
        }

        intid += ints_prop->int_entries[idx].data[1];

        intids[idx] = intid;
    }
}

static void gicv2_dtb_setup_intids(void* ctx, dt_prop_ints_t* ints_prop) {
    gicv2_ctx_t* gic = ctx;

    for (uint64_t idx = 0; idx < ints_prop->num_ints; idx++) {
        uint64_t intid = 0;
        if (ints_prop->int_entries[idx].data[0] == 0) {
            // SPI interrupt. Add SPI offset
            intid = GICv2_INTID_SPI_BASE;
        } else {
            // PPI interrupt. Add PPI offset
            intid = GICv2_INTID_PPI_BASE;
        }

        intid += ints_prop->int_entries[idx].data[1];

        uint32_t trigger = ints_prop->int_entries[idx].data[2];

        ASSERT(trigger != 0);

        if ((trigger & 0x3) != 0) {
            gicv2_set_spi_trigger(gic, intid, INTERRUPT_TRIGGER_EDGE);
        } else {
            gicv2_set_spi_trigger(gic, intid, INTERRUPT_TRIGGER_LEVEL);
        }
    }
}

static intc_dtb_funcs_t intc_dtb_funcs = {
    .get_intid_list = gicv2_dtb_get_intid_list,
    .setup_intids = gicv2_dtb_setup_intids
};

void gicv2_irq_handler(void* ctx) {
    gicv2_ctx_t* gic = ctx;

    uint64_t iar_virt = (uintptr_t)&gic->gicc->iar;
    asm volatile ("dc IVAC, %0"
                  :
                  : "r" (iar_virt));
    uint32_t iar = gic->gicc->iar;
    uint32_t intid = (iar & GICCv2_IAR_INTID_MASK) >> GICCv2_IAR_INTID_SHIFT;

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    if (intid != 1023) {

        gic->gicd->icpendr[intid_word] = BIT(intid_bit);

        interrupt_handle_irq(intid);

        gic->gicc->eoir = iar;

        if (gic->must_deactivate) {
            MEM_DMB();
            gic->gicc->dir = iar;
        }
    }
}

void gicv2_discover(void* ctx) {

    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    ASSERT(dt_node->prop_reg != NULL);

    dt_prop_reg_entry_t* gic_reg_entries = dt_node->prop_reg->reg_entries;

    ASSERT(gic_reg_entries[DTB_REG_IDX_GICDv2].addr_size <= 2);
    ASSERT(gic_reg_entries[DTB_REG_IDX_GICDv2].size_size <= 2);

    uint64_t gicd_addr_bus;
    uint64_t gicd_size;
    uint64_t gicc_addr_bus;
    uint64_t gicc_size;
    if (gic_reg_entries[DTB_REG_IDX_GICDv2].addr_size == 1) {
        gicd_addr_bus = gic_reg_entries[DTB_REG_IDX_GICDv2].addr_ptr[0]; 
        gicc_addr_bus = gic_reg_entries[DTB_REG_IDX_GICCv2].addr_ptr[0]; 
    } else {
        gicd_addr_bus = gic_reg_entries[DTB_REG_IDX_GICDv2].addr_ptr[1] | (((uint64_t)gic_reg_entries[DTB_REG_IDX_GICDv2].addr_ptr[0]) << 32); 
        gicc_addr_bus = gic_reg_entries[DTB_REG_IDX_GICCv2].addr_ptr[1] | (((uint64_t)gic_reg_entries[DTB_REG_IDX_GICCv2].addr_ptr[0]) << 32); 
    }

    if (gic_reg_entries[DTB_REG_IDX_GICDv2].size_size == 1) {
        gicd_size = gic_reg_entries[DTB_REG_IDX_GICDv2].size_ptr[0];
        gicc_size = gic_reg_entries[DTB_REG_IDX_GICCv2].size_ptr[0];

    } else {
        gicd_size = gic_reg_entries[DTB_REG_IDX_GICDv2].size_ptr[1] | (((uint64_t)gic_reg_entries[DTB_REG_IDX_GICDv2].size_ptr[0]) << 32);
        gicc_size = gic_reg_entries[DTB_REG_IDX_GICCv2].size_ptr[1] | (((uint64_t)gic_reg_entries[DTB_REG_IDX_GICCv2].size_ptr[0]) << 32);
    }

    bool gicd_valid;
    bool gicc_valid;

    uint64_t gicd_addr_phy = dt_map_addr_to_phy(dt_node, gicd_addr_bus, &gicd_valid);
    uint64_t gicc_addr_phy = dt_map_addr_to_phy(dt_node, gicc_addr_bus, &gicc_valid);

    ASSERT(gicd_valid);
    ASSERT(gicc_valid);

    gicv2_ctx_t* gic = vmalloc(sizeof(gicv2_ctx_t));

    //gic->gicd = PHY_TO_KSPACE_PTR(0xFF841000);
    //gic->gicc = PHY_TO_KSPACE_PTR(0xFF842000);
    //gic->gicd = PHY_TO_KSPACE_PTR(0x4C0041000);
    //gic->gicc = PHY_TO_KSPACE_PTR(0x4C0042000);

    gic->gicd = PHY_TO_KSPACE_PTR(gicd_addr_phy);
    gic->gicc = PHY_TO_KSPACE_PTR(gicc_addr_phy);

    memspace_map_device_kernel((void*)gicd_addr_phy,
                               PHY_TO_KSPACE_PTR(gicd_addr_phy),
                               gicd_size,
                               MEMSPACE_FLAG_PERM_KRW);
    memspace_map_device_kernel((void*)gicc_addr_phy,
                               PHY_TO_KSPACE_PTR(gicc_addr_phy),
                               gicc_size,
                               MEMSPACE_FLAG_PERM_KRW);
    memspace_update_kernel_vmem();

    gicv2_disable(gic);

    for (uint64_t idx = 0; idx < GICv2_INTID_SPI_LIMIT/16; idx++) {
        gic->gicd->icfgr[idx] = 0;
    }

    console_log(LOG_INFO, "Discovered GICv2");
    console_log(LOG_DEBUG, " GICD: %16x", (uint64_t)gic->gicd);
    console_log(LOG_DEBUG, " GICC: %16x", (uint64_t)gic->gicc);
    console_log(LOG_DEBUG, " GICD_TYPER: %8x", gic->gicd->typer);
    console_log(LOG_DEBUG, " GICD_IIDR: %8x", gic->gicd->iidr);
    console_log(LOG_DEBUG, " GICC_IIDR: %8x", gic->gicc->iidr);
    console_log(LOG_DEBUG, " GICC_CTLR: %8x", gic->gicc->ctlr);

    uint32_t lines = (gic->gicd->typer & GICDv2_TYPER_ITLINESNUMBER_MASK) >> GICDv2_TYPER_ITLINESNUMBER_SHIFT;
    gic->max_intid = 32 * (lines + 1);
    console_log(LOG_INFO, " Max INTID: %d", gic->max_intid);

    gic->must_deactivate = (gic->gicc->ctlr & GICCv2_CTLR_EOIMODENS) != 0;

    dt_node->dtb_ctx = gic;
    dt_node->dtb_funcs = &intc_dtb_funcs;

    bitalloc_init(&gic->intid_alloc, GICv2_INTID_SPI_BASE, gic->max_intid, vmalloc);

    interrupt_register_controller(&intc_funcs, gic, GICv2_INTID_SPI_LIMIT);
}

void gicv2_register(void) {
    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "arm,gic-400"
        },
        .ctxfunc = gicv2_discover
    };
    register_driver(&reg);

}

REGISTER_DRIVER(gicv2)