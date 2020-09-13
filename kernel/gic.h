
#ifndef __GIC_H__
#define __GIC_H__

#include <stdint.h>

#include "kernel/bitutils.h"

void gic_init(void);

void gic_enable(void);

void gic_enable_intid(uint32_t intid);
void gic_disable_intid(uint32_t intid);

typedef void (*irq_handler)(uint32_t intid);


#define GIC_MAX_IRQ_WORDS (0x80/4)
#define GIC_MAX_SGI_WORDS (0x10/4)

typedef struct __attribute__((__packed__)) {

    volatile uint32_t ctlr;
    volatile uint32_t typer;
    volatile uint32_t iidr;
    volatile uint32_t res_0;
    volatile uint32_t statusr;
    volatile uint32_t res_1[11];
    volatile uint32_t setspi_nsr;
    volatile uint32_t res_2;
    volatile uint32_t clrspi_nsr;
    volatile uint32_t res_3;
    volatile uint32_t setspi_sr;
    volatile uint32_t res_4;
    volatile uint32_t clrspi_sr;
    volatile uint32_t res_5[9];
    volatile uint32_t igroupr[GIC_MAX_IRQ_WORDS];
    volatile uint32_t isenabler[GIC_MAX_IRQ_WORDS];
    volatile uint32_t icenabler[GIC_MAX_IRQ_WORDS];
    volatile uint32_t ispendr[GIC_MAX_IRQ_WORDS];
    volatile uint32_t icpendr[GIC_MAX_IRQ_WORDS];
    volatile uint32_t isactiver[GIC_MAX_IRQ_WORDS];
    volatile uint32_t icactiver[GIC_MAX_IRQ_WORDS];
    volatile uint32_t ipriorityr[GIC_MAX_IRQ_WORDS*8];
    volatile uint32_t itargetsr[GIC_MAX_IRQ_WORDS*8];
    volatile uint32_t icfgr[GIC_MAX_IRQ_WORDS*2];
    volatile uint32_t igrpmodr[GIC_MAX_IRQ_WORDS];
    volatile uint32_t nsacr[GIC_MAX_IRQ_WORDS*2];
    volatile uint32_t sgir;
    volatile uint32_t res_6[3];
    volatile uint32_t cpendsgir[GIC_MAX_SGI_WORDS];
    volatile uint32_t spendsgir[GIC_MAX_SGI_WORDS];
    volatile uint32_t res_7[0x1474];
    volatile uint64_t irouter[0x3DC];
} GICD_Struct;

typedef struct __attribute__((__packed__)) {
    volatile uint32_t ctlr;
    volatile uint32_t pmr;
    volatile uint32_t bpr;
    volatile uint32_t iar;
    volatile uint32_t eoir;
    volatile uint32_t rpr;
    volatile uint32_t hppir;
    volatile uint32_t abpr;
    volatile uint32_t aiar;
    volatile uint32_t aeoir;
    volatile uint32_t ahppir;
    volatile uint32_t staturs;
    volatile uint32_t res_0[0x28];
    volatile uint32_t apr[4];
    volatile uint32_t nsapr[4];
    volatile uint32_t res_1[2];
    volatile uint32_t iidr;
    volatile uint32_t res_2[0x3C0];
    volatile uint32_t dir;
} GICC_Struct;

#define GICD_BASE_PHYS ((GICD_Struct*) 0x08000000)
#define GICD_BASE_VIRT ((GICD_Struct*) 0xFFFF000008000000)

#define GICC_BASE_PHYS ((GICC_Struct*) 0x08010000)
#define GICC_BASE_VIRT ((GICC_Struct*) 0xFFFF000008010000)


#define GICD_CTRL_RWP BIT(31)
#define GICD_CTRL_ARE_NS BIT(4)
#define GICD_CTRL_ENG1 BIT(1)
#define GICD_CTRL_ENG0 BIT(0)



#define GICC_CTRL_EOIMODENS BIT(9)
#define GICC_CTRL_IRQBYPDISG1 BIT(9)
#define GICC_CTRL_FIQBYPDISG1 BIT(9)
#define GICC_CTRL_ENG1 BIT(9)

#endif

