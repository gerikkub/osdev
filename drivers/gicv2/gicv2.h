
#ifndef __GIC_H__
#define __GIC_H__

#include <stdint.h>

#include "stdlib/bitutils.h"


void gicv2_init(void);

void gicv2_enable(void* ctx);

void gicv2_set_irq_handler(irq_handler handler, uint32_t intid);
void gicv2_enable_intid(void* ctx, uint64_t intid);
void gicv2_disable_intid(void* ctx, uint64_t intid);

void gicv2_irq_handler(void* ctx);

void gicv2_poll_msi(void* ctx);

#define GICv2_MAX_IRQS (1024)

#define GICv2_MAX_IRQ_WORDS (32)

typedef struct __attribute__((__packed__)) {

    uint32_t ctlr;
    uint32_t typer;
    uint32_t iidr;
    uint32_t res_0[29];
    uint32_t igroupr[GICv2_MAX_IRQ_WORDS];
    uint32_t isenabler[GICv2_MAX_IRQ_WORDS];
    uint32_t icenabler[GICv2_MAX_IRQ_WORDS];
    uint32_t ispendr[GICv2_MAX_IRQ_WORDS];
    uint32_t icpendr[GICv2_MAX_IRQ_WORDS];
    uint32_t isactiver[GICv2_MAX_IRQ_WORDS];
    uint32_t icactiver[GICv2_MAX_IRQ_WORDS];
    uint32_t ipriorityr[GICv2_MAX_IRQ_WORDS*8 - 1];
    uint32_t res_1;
    uint32_t itargetsr[GICv2_MAX_IRQ_WORDS*8 - 1];
    uint32_t res_2;
    uint32_t icfgr[GICv2_MAX_IRQ_WORDS*2];
    uint32_t impl_0[32];
    uint32_t nsacr[GICv2_MAX_IRQ_WORDS*2];
    uint32_t sgir;
    uint32_t res_3[3];
    uint32_t cpendsgir[4];
    uint32_t spendsgir[4];
    uint32_t res_13[28];
    uint32_t impl_1[12];
} GICDv2_Struct;

typedef struct __attribute__((__packed__)) {
    uint32_t ctlr;
    uint32_t pmr;
    uint32_t bpr;
    uint32_t iar;
    uint32_t eoir;
    uint32_t rpr;
    uint32_t hppir;
    uint32_t abpr;
    uint32_t aiar;
    uint32_t aeoir;
    uint32_t ahppir;
    uint32_t res_0[5];
    uint32_t impl_0[36];
    uint32_t apr[4];
    uint32_t nsapr[4];
    // Note: GICv2 mentions that this register starts at 0xED, which is not aligned 4. Mistake?
    uint32_t res_1[3]; 
    uint32_t iidr;
    uint32_t dir;
} GICCv2_Struct;

enum {
    DTB_REG_IDX_GICDv2 = 0,
    DTB_REG_IDX_GICCv2 = 1,
    DTB_REG_IDX_GICHv2 = 2,
    DTB_REG_IDX_GICVv2 = 3,
    DTB_REG_IDX_Max
};

#define GICv2_INTID_SGI_BASE (0)
#define GICv2_INTID_SGI_LIMIT (16)
#define GICv2_INTID_PPI_BASE (16)
#define GICv2_INTID_PPI_LIMIT (32)
#define GICv2_INTID_SPI_BASE (32)
#define GICv2_INTID_SPI_LIMIT (1029)

#define GICDv2_CTLR_ENABLE BIT(0)

#define GICDv2_TYPER_LSPI_MASK (0x1F)
#define GICDv2_TYPER_LSPI_SHIFT (11)
#define GICDv2_TYPER_SECURITYEXTN BIT(10)
#define GICDv2_TYPER_CPUNUMBER_MASK (0x7)
#define GICDv2_TYPER_CPUNUMBER_SHIFT (5)
#define GICDv2_TYPER_ITLINESNUMBER_MASK (0x1F)
#define GICDv2_TYPER_ITLINESNUMBER_SHIFT (0)

#define GICDv2_IIDR_IMPLEMENTER_MASK (0xFFF)
#define GICDv2_IIDR_IMPLEMENTER_SHIFT (0)
#define GICDv2_IIDR_REVISION_MASK (0xF)
#define GICDv2_IIDR_REVISION_SHIFT (12)
#define GICDv2_IIDR_ARCH_VERSION_MASK (0xF)
#define GICDv2_IIDR_ARCH_VERSION_SHIFT (16)
#define GICDv2_IIDR_PRODUCT_ID_MASK (0xFFF)
#define GICDv2_IIDR_PRODUCT_ID_SHIFT (20)

#define GICCv2_CTLR_EOIMODENS BIT(9)
#define GICCv2_CTLR_IRQBYPDISGRP1 BIT(6)
#define GICCv2_CTLR_FIQBYPDISGRP1 BIT(5)
#define GICCv2_CTLR_ENABLEGRP1 BIT(0)

#define GICCv2_IAR_INTID_MASK (0x3FF)
#define GICCv2_IAR_INTID_SHIFT (0)
#define GICCv2_IAR_CPUID_MASK (0x7)
#define GICCv2_IAR_CPUID_SHIFT (10)

#define GICCv2_EOIR_EOIINTID_MASK (0x3FF)
#define GICCv2_EOIR_EOIINTID_SHIFT (0)
#define GICCv2_EOIR_CPUID_MASK (0x7)
#define GICCv2_EOIR_CPUID_SHIFT (10)

#define GICCv2_HPPIR_PENDINTID_MASK (0x3FF)
#define GICCv2_HPPIR_PENDINTID_SHIFT (0)
#define GICCv2_HPPIR_CPUID_MASK (0x7)
#define GICCv2_HPPIR_CPUID_SHIFT (10)

#define GICCv2_AIAR_INTID_MASK (0x3FF)
#define GICCv2_AIAR_INTID_SHIFT (0)
#define GICCv2_AIAR_CPUID_MASK (0x7)
#define GICCv2_AIAR_CPUID_SHIFT (10)

#define GICCv2_AEOIR_EOIINTID_MASK (0x3FF)
#define GICCv2_AEOIR_EOIINTID_SHIFT (0)
#define GICCv2_AEOIR_CPUID_MASK (0x7)
#define GICCv2_AEOIR_CPUID_SHIFT (10)

#define GICCv2_AHPPIR_PENDINTID_MASK (0x3FF)
#define GICCv2_AHPPIR_PENDINTID_SHIFT (0)
#define GICCv2_AHPPIR_CPUID_MASK (0x7)
#define GICCv2_AHPPIR_CPUID_SHIFT (10)

#define GICCv2_IIDR_IMPLEMENTER_MASK (0xFFF)
#define GICCv2_IIDR_IMPLEMENTER_SHIFT (0)
#define GICCv2_IIDR_REVISION_MASK (0xF)
#define GICCv2_IIDR_REVISION_SHIFT (12)
#define GICCv2_IIDR_ARCH_VERSION_MASK (0xF)
#define GICCv2_IIDR_ARCH_VERSION_SHIFT (16)
#define GICCv2_IIDR_PRODUCT_ID_MASK (0xFFF)
#define GICCv2_IIDR_PRODUCT_ID_SHIFT (20)

#define GICCv2_DIR_INTID_MASK (0x3FF)
#define GICCv2_DIR_INTID_SHIFT (0)
#define GICCv2_DIR_CPUID_MASK (0x7)
#define GICCv2_DIR_CPUID_SHIFT (10)

#endif

