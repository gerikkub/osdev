
#ifndef __VMEM_BOOTSTRAP_H__
#define __VMEM_BOOTSTRAP_H__

#ifdef __VMEM_H__
#error "Both vmem.h and vmem_bootstrap.h have been defined"
#endif

#include <stdint.h>

#include "kernel/bitutils.h"

#define VMEM_4K_TABLE_ENTRIES (1 << 9)

typedef uint64_t _vmem_entry_invalid_t;
typedef uint64_t _vmem_entry_block_t ;
typedef uint64_t _vmem_entry_table_t ;
typedef uint64_t _vmem_entry_page_t ;

typedef uint64_t _vmem_entry_t;

typedef _vmem_entry_t _vmem_table;

typedef uintptr_t addr_phy_t;
typedef uintptr_t addr_virt_t;

typedef uint64_t _vmem_ap_flags;


#define VMEM_STG1_TBL_NSTABLE BIT(63)
#define VMEM_STG1_TBL_APTABLE1 BIT(62)
#define VMEM_STG1_TBL_APTABLE0 BIT(61)
#define VMEM_STG1_TBL_UXNTABLE BIT(60)
#define VMEM_STG1_TBL_PXNTABLE BIT(59)

#define VMEM_STG1_BLK_UXN BIT(54)
#define VMEM_STG1_BLK_PXN BIT(53)
#define VMEM_STG1_BLK_CONT BIT(52)
#define VMEM_STG1_BLK_NG BIT(11)
#define VMEM_STG1_BLK_AF BIT(10)
#define VMEM_STG1_BLK_SH1 BIT(9)
#define VMEM_STG1_BLK_SH0 BIT(8)
#define VMEM_STG1_BLK_AP2 BIT(7)
#define VMEM_STG1_BLK_AP1 BIT(6)
#define VMEM_STG1_BLK_NS BIT(5)
#define VMEM_STG1_BLK_ATTRIDX2 BIT(4)
#define VMEM_STG1_BLK_ATTRIDX1 BIT(3)
#define VMEM_STG1_BLK_ATTRIDX0 BIT(2)

#define SYS_TCR_EL1_TBI1 BIT(38)
#define SYS_TCR_EL1_TBI0 BIT(37)
#define SYS_TCR_EL1_AS BIT(36)
#define SYS_TCR_EL1_IPS2 BIT(34)
#define SYS_TCR_EL1_IPS1 BIT(33)
#define SYS_TCR_EL1_IPS0 BIT(32)
#define SYS_TCR_EL1_TG11 BIT(31)
#define SYS_TCR_EL1_TG12 BIT(30)
#define SYS_TCR_EL1_SH11 BIT(29)
#define SYS_TCR_EL1_SH10 BIT(28)
#define SYS_TCR_EL1_ORGN11 BIT(27)
#define SYS_TCR_EL1_ORGN10 BIT(26)
#define SYS_TCR_EL1_IRGN11 BIT(25)
#define SYS_TCR_EL1_IRGN10 BIT(24)
#define SYS_TCR_EL1_EPD1 BIT(23)
#define SYS_TCR_EL1_A1 BIT(22)
#define SYS_TCR_EL1_T1SZ5 BIT(21)
#define SYS_TCR_EL1_T1SZ4 BIT(20)
#define SYS_TCR_EL1_T1SZ3 BIT(19)
#define SYS_TCR_EL1_T1SZ2 BIT(18)
#define SYS_TCR_EL1_T1SZ1 BIT(17)
#define SYS_TCR_EL1_T1SZ0 BIT(16)
#define SYS_TCR_EL1_T1SZ(x) (((x) & 0x1F) << 16)
#define SYS_TCR_EL1_TG01 BIT(15)
#define SYS_TCR_EL1_TG00 BIT(14)
#define SYS_TCR_EL1_SH01 BIT(13)
#define SYS_TCR_EL1_SH00 BIT(12)
#define SYS_TCR_EL1_ORGN01 BIT(11)
#define SYS_TCR_EL1_ORGN00 BIT(10)
#define SYS_TCR_EL1_IRGN01 BIT(9)
#define SYS_TCR_EL1_IRGN00 BIT(8)
#define SYS_TCR_EL1_EPD0 BIT(7)
#define SYS_TCR_EL1_T0SZ5 BIT(5)
#define SYS_TCR_EL1_T0SZ4 BIT(4)
#define SYS_TCR_EL1_T0SZ3 BIT(3)
#define SYS_TCR_EL1_T0SZ2 BIT(2)
#define SYS_TCR_EL1_T0SZ1 BIT(1)
#define SYS_TCR_EL1_T0SZ0 BIT(0)
#define SYS_TCR_EL1_T0SZ(x) ((x) & 0x1F)


#define VMEM_ATTR_NORMAL_NONCACHE (0)
#define VMEM_ATTR_DEVICE (VMEM_STG1_BLK_ATTRIDX0)

#define VMEM_AP_ALLOW_UX(x) (((x) & VMEM_AP_U_E) != 0)
#define VMEM_AP_ALLOW_PX(x) (((x) & VMEM_AP_P_E) != 0)

#define VMEM_AP_EXTRACT(x) (0) //TODO

enum {
    VMEM_AP_U_R = 1,
    VMEM_AP_U_W = 2,
    VMEM_AP_U_E = 4,
    VMEM_AP_P_R = 8,
    VMEM_AP_P_W = 16,
    VMEM_AP_P_E = 32
};

typedef enum {
    VMEM_ATTR_MEM = 0,
    VMEM_ADDR_DEVICE
} vmem_attr_t;


void setup_vmem_bootstrap(void);

#endif
