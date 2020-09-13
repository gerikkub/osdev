

#include <stdint.h>

#include "kernel/gic.h"
#include "kernel/bitutils.h"
#include "kernel/assert.h"
#include "kernel/console.h"

void gic_init(void) {

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

    ASSERT(intid < (GIC_MAX_IRQ_WORDS * 32));

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    GICD_BASE_VIRT->isenabler[intid_word] = BIT(intid_bit);
}

void gic_disable_intid(uint32_t intid) {

    ASSERT(intid < (GIC_MAX_IRQ_WORDS * 32));

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    GICD_BASE_VIRT->icenabler[intid_word] = BIT(intid_bit);
}

volatile uint8_t gic_got_irq = 0;

void gic_irq_handler(uint32_t vector) {

    uint32_t intid = GICC_BASE_VIRT->iar;

    uint32_t intid_word = intid / 32;
    uint32_t intid_bit = intid % 32;

    GICD_BASE_VIRT->icpendr[intid_word] = BIT(intid_bit);

    uint32_t zero = 0;
    WRITE_SYS_REG(CNTP_CTL_EL0, zero);

    GICC_BASE_VIRT->eoir = intid;

    gic_got_irq = 1;

    //console_write("Got IRQ ");
    //console_write_num(intid);
    //console_endl();

    //gtimer_start_downtimer(62500000, true);
}
