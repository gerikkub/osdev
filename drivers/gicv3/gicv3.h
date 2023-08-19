
#ifndef __GIC_H__
#define __GIC_H__

#include <stdint.h>

#include "stdlib/bitutils.h"


void gic_init(void);

void gic_enable(void);

void gic_set_irq_handler(irq_handler handler, uint32_t intid);
void gic_enable_intid(uint32_t intid);
void gic_disable_intid(uint32_t intid);

bool gic_try_irq_handler(void);
void gic_irq_handler(uint32_t vector);

void gicv3_poll_msi(void);

void gicv3_register_its(void* its_ctx);

#define GIC_MAX_IRQS (1024)

#define GIC_MAX_IRQ_WORDS (0x80/4)
#define GIC_MAX_SGI_WORDS (0x10/4)

typedef struct __attribute__((__packed__)) {

    uint32_t ctlr;
    uint32_t typer;
    uint32_t iidr;
    uint32_t res_0;
    uint32_t statusr;
    uint32_t res_1[11];
    uint32_t setspi_nsr;
    uint32_t res_2;
    uint32_t clrspi_nsr;
    uint32_t res_3;
    uint32_t setspi_sr;
    uint32_t res_4;
    uint32_t clrspi_sr;
    uint32_t res_5[9];
    uint32_t igroupr[GIC_MAX_IRQ_WORDS];
    uint32_t isenabler[GIC_MAX_IRQ_WORDS];
    uint32_t icenabler[GIC_MAX_IRQ_WORDS];
    uint32_t ispendr[GIC_MAX_IRQ_WORDS];
    uint32_t icpendr[GIC_MAX_IRQ_WORDS];
    uint32_t isactiver[GIC_MAX_IRQ_WORDS];
    uint32_t icactiver[GIC_MAX_IRQ_WORDS];
    uint32_t ipriorityr[GIC_MAX_IRQ_WORDS*8];
    uint32_t itargetsr[GIC_MAX_IRQ_WORDS*8];
    uint32_t icfgr[GIC_MAX_IRQ_WORDS*2];
    uint32_t igrpmodr[GIC_MAX_IRQ_WORDS];
    uint32_t nsacr[GIC_MAX_IRQ_WORDS*2];
    uint32_t sgir;
    uint32_t res_6[3];
    uint32_t cpendsgir[GIC_MAX_SGI_WORDS];
    uint32_t spendsgir[GIC_MAX_SGI_WORDS];
    uint32_t res_7[0x1474];
    uint64_t irouter[0x3DC];
} GICD_Struct;

typedef struct __attribute__((__packed__)) {
    uint32_t ctlr;
    uint32_t iidr;
    uint32_t typer;
    uint32_t statusr;
    uint32_t waker;
    uint32_t res0[2];
    uint32_t impldef0[8];
    uint64_t setlpir;
    uint64_t clrlpir;
    uint32_t res1[8];
    uint64_t propbaser;
    uint64_t pendbaser;
    uint32_t res2[8];
    uint64_t invlpir;
    uint64_t res3;
    uint64_t invallr;
    uint64_t res4;
    uint64_t syncr;
} GICR_Struct;

typedef struct __attribute__((__packed__)) {
    uint32_t res0[16];
    uint32_t impldef0[4];
    uint32_t res1[8];
    uint64_t vpropbaser;
    uint64_t vpendbaser;
} GICR_Virtual_Struct;

typedef struct __attribute__((__packed__)) {
    uint32_t res0[32];
    uint32_t igroupr0;
    uint32_t res1[31];
    uint32_t isenabler0;
    uint32_t res2[31];
    uint32_t icenabler0;
    uint32_t res3[31];
    uint32_t ispendr0;
    uint32_t res4[31];
    uint32_t icpendr0;
    uint32_t res5[31];
    uint32_t iprorityr[8];
    uint32_t res9[504];
    uint32_t icfgr0;
    uint32_t icfgr1;
    uint32_t res10[62];
    uint32_t igrpmodr0;
    uint32_t res11[63];
    uint32_t nsacr;
} GICR_PPI_Struct;

/*
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
    uint32_t staturs;
    uint32_t res_0[0x28];
    uint32_t apr[4];
    uint32_t nsapr[4];
    uint32_t res_1[2];
    uint32_t iidr;
    uint32_t res_2[0x3C0];
    uint32_t dir;
} GICC_Struct;
*/

enum {
    DTB_REG_IDX_GICD = 0,
    //DTB_REG_IDX_GICR = 1, // Need to better understand how large this is
    DTB_REG_IDX_GICR = 1,
    DTB_REG_IDX_Max
};

#define GIC_INTID_SGI_BASE (0)
#define GIC_INTID_SGI_LIMIT (16)
#define GIC_INTID_PPI_BASE (16)
#define GIC_INTID_PPI_LIMIT (32)
#define GIC_INTID_SPI_BASE (32)
#define GIC_INTID_SPI_LIMIT (1029)
#define GIC_INTID_LPI_BASE (8192)
// TODO: What is this limit?
#define GIC_INTID_LPI_LIMIT (9000)

#define GICD_CTRL_RWP BIT(31)
#define GICD_CTRL_E1NWF BIT(7)
#define GICD_CTRL_DS BIT(6)
#define GICD_CTRL_ARE BIT(4)
#define GICD_CTRL_ENG1 BIT(1)
#define GICD_CTRL_ENG0 BIT(0)



#define ICC_CTRL_A3V BIT(15)
#define ICC_CTRL_SEIS BIT(14)
#define ICC_CTRL_IDBITS_SHIFT (11)
#define ICC_CTRL_IDBITS_MASK (0x7)
#define ICC_CTRL_PRIBITS_SHIFT (8)
#define ICC_CTRL_PRIBITS_MASK (0x7)
#define ICC_CTRL_PMHE BIT(6)
#define ICC_CTRL_EOIMODE BIT(1)
#define ICC_CTRL_CBPR BIT(0)

#define ICC_IGRPEN1_ENABLE BIT(0)

#define ICC_REG_CTLR_EL1 s3_0_c12_c12_4
#define ICC_REG_IGRPEN1_EL1 s3_0_c12_c12_7
#define ICC_REG_PMR_EL1 s3_0_c4_c6_0
#define ICC_REG_BPR1_EL1 s3_0_c12_c12_3
#define ICC_REG_IAR1_EL1 s3_0_c12_c12_0
#define ICC_REG_EOIR1_EL1 s3_0_c12_c12_1

/*
#define GICD_CLEAR_ACTIVE(intid) \
    GICD_BASE_VIRT->icpendr[(intid/32)] = BIT(inid % 32);

#define GICD_MSI_ADDR (&GICD_BASE_VIRT->SETSPI_NSR)
*/

#endif

