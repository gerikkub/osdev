

#include <stdint.h>

#include "kernel/gic.h"
#include "kernel/bitutils.h"
#include "kernel/assert.h"
#include "kernel/console.h"

static irq_handler s_irq_handlers[GIC_MAX_IRQS] = {0};

void gic_init(void) {

}

void gic_set_irq_handler(irq_handler handler, uint32_t intid) {

    ASSERT(handler != NULL);
    ASSERT(intid < GIC_MAX_IRQS);

    ASSERT(s_irq_handlers[intid] == NULL);

    s_irq_handlers[intid] = handler;
}

void gic_enable(void) {

    GICC_Struct* gicc = GICC_BASE_VIRT;
    GICD_Struct* gicd = GICD_BASE_VIRT;

    // Enable the Distributor
    gicc->ctlr = 1;
    gicd->ctlr = 1;

    gicc->pmr = 0xFF;

    gicc->bpr = 0;
}

void gic_enable_intid(uint32_t intid) {

    ASSERT(intid < GIC_MAX_IRQS);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    GICD_BASE_VIRT->isenabler[intid_word] = BIT(intid_bit);
}

void gic_disable_intid(uint32_t intid) {

    ASSERT(intid < GIC_MAX_IRQS);

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    GICD_BASE_VIRT->icenabler[intid_word] = BIT(intid_bit);
}

void gic_irq_handler(uint32_t vector) {

    uint32_t intid = GICC_BASE_VIRT->iar;

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    GICD_BASE_VIRT->icpendr[intid_word] = BIT(intid_bit);

    if (s_irq_handlers[intid] != NULL) {
        s_irq_handlers[intid](intid);
    }

    GICC_BASE_VIRT->eoir = intid;
}

