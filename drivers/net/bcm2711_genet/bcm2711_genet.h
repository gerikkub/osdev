
#ifndef __DRIVERS_NET_BCM2711_GENET_H__
#define __DRIVERS_NET_BCM2711_GENET_H__

#include <stdint.h>

#define GENET_DMA_DESC_COUNT (256)
#define GENET_DMA_DESC_SIZE (12)

typedef struct __attribute__((packed)) {
    // Buffer Length and Flags
#define BCM2711_GENET_DMA_DESC_LENGTH_MASK (0xfff)
#define BCM2711_GENET_DMA_DESC_LENGTH_SHIFT (16)

#define BCM2711_GENET_DMA_RX_DESC_STATUS_OWN BIT(15)
#define BCM2711_GENET_DMA_RX_DESC_STATUS_CHECKSUM_OK BIT(15)
#define BCM2711_GENET_DMA_RX_DESC_STATUS_EOP BIT(14)
#define BCM2711_GENET_DMA_RX_DESC_STATUS_SOP BIT(13)
#define BCM2711_GENET_DMA_RX_DESC_STATUS_RX_ERROR BIT(2)

#define BCM2711_GENET_DMA_TX_DESC_STATUS_OWN BIT(15)
#define BCM2711_GENET_DMA_TX_DESC_STATUS_EOP BIT(14)
#define BCM2711_GENET_DMA_TX_DESC_STATUS_SOP BIT(13)
#define BCM2711_GENET_DMA_TX_DESC_STATUS_QTAG_MASK (0x3f)
#define BCM2711_GENET_DMA_TX_DESC_STATUS_QTAG_SHIFT (7)
#define BCM2711_GENET_DMA_TX_DESC_STATUS_CRC BIT(6)
#define BCM2711_GENET_DMA_TX_DESC_STATUS_CHECKSUM BIT(4)
    uint32_t length_status;

    // PHY address of buffer???
    uint32_t addr_lo;
    uint32_t addr_hi;
} BCM2711GenetDmaDesc_t;

typedef struct __attribute__((packed)) {
    // Unknown. Write 0 on intialization
    uint32_t write_ptr_lo;
    // Unknown. Write 0 on intialization
    uint32_t write_ptr_hi;

    // Writen by the DMA to indicate that a
    // descriptor has been written to. Points
    // to the next descriptor that will be written
    // If prod_index == cons_index then there is
    // no data waiting to be read
    uint32_t prod_index;

    // Writen by the driver to indicate that a
    // buffer has been processed. 
    uint32_t cons_index;

    // The number of descriptors in the ring and 
    // The max size of a single buffer??? FreeBSD sets to 2048 bytes
    uint32_t ring_size;

    // Unknown. Write 0 on intialization
    uint32_t start_addr_lo;
    // Unknown. Write 0 on intialization
    uint32_t start_addr_hi;
    // Unknown. FreeBSD sets to the following.
    // (DMA_DESCRIPTOR_COUNT * DMA_DESCRIPTOR_SIZE) / 4 - 1
    // (256 * 32) / 4 - 1
    uint32_t end_addr_lo;
    // Unknown. Write 0 on intialization
    uint32_t end_addr_hi;

    uint32_t res0;

    // DMA Flow Control???
    // FreeBSD sets to
    // (5 << 16 | DMA_DESCRIPTOR_COUNT >> 4)
    uint32_t done_thresh;


    // Unknown. Write 0 on intialization
    uint32_t read_ptr_lo;
    // Unknown. Write 0 on intialization
    uint32_t read_ptr_hi;

    uint32_t res1[3];
} BCM2711GenetRxDmaRing_t;

typedef struct __attribute__((packed)) {
    uint32_t read_ptr_lo;
    uint32_t read_ptr_hi;
    uint32_t cons_index;
    uint32_t prod_index;

    uint32_t ring_size;
    uint32_t start_addr_lo;
    uint32_t start_addr_hi;
    uint32_t end_addr_lo;
    uint32_t end_addr_hi;

    uint32_t mbuf_done_thresh;
    uint32_t flow_period;
    uint32_t write_ptr_lo;
    uint32_t write_ptr_hi;

    uint32_t res1[3];
} BCM2711GenetTxDmaRing_t;

typedef struct __attribute__((packed)) {
    uint32_t sys_rev_ctrl;
    uint32_t port_ctrl;
    uint32_t rbuf_flush_ctrl;
    uint32_t tbuf_flush_ctrl;
    uint32_t res0[4*7 + 3];
    uint32_t rgmii_oob_ctrl;
    uint32_t res1[4*23];

    // 0x200
    uint32_t intrl2_cpu_stat;
    uint32_t intrl2_cpu_set;
    uint32_t intrl2_cpu_clear;
    uint32_t intrl2_cpu_mask_stat;
    uint32_t intrl2_cpu_mask_set;
    uint32_t intrl2_cpu_mask_clear;
    uint32_t res3[4*14 + 2];

    // 0x300
    uint32_t rbuf_ctrl;
    uint32_t res4[4];
    uint32_t check_ctrl;
    uint32_t res5[39];
    uint32_t rbuf_tbuf_size_ctrl;
    uint32_t res6[146];

    // 0x600
    uint32_t tbuf_ctrl;
    uint32_t res7[127];

    // 0x800
    uint32_t res8[2];
    uint32_t umac_cmd;
    uint32_t umac_mac0;
    uint32_t umac_mac1;
    uint32_t umac_max_frame_len;
    uint32_t res9[199];

    // 0xB34
    uint32_t tx_flush;
    uint32_t res10[146];

    // 0xd80
    uint32_t umac_mib_ctrl;
    uint32_t res11[36];

    // 0xe14
    uint32_t mdio_cmd;
    uint32_t res12[14];

    // 0xe50
    uint32_t umac_mdf_ctrl;
    uint32_t res13[107];

    // 0x1000
    uint32_t res14[1024];

    // 0x2000
    BCM2711GenetDmaDesc_t rx_desc[256];
    BCM2711GenetRxDmaRing_t rx_ring[17];
    uint32_t rx_dma_ring_cfg;
    uint32_t rx_dma_ctrl;
    uint32_t res15;
    uint32_t rx_scb_burst_size;
    uint32_t res16[1004];

    // 0x4000
    BCM2711GenetDmaDesc_t tx_desc[256];
    BCM2711GenetTxDmaRing_t tx_ring[17];
    uint32_t tx_dma_ring_cfg;
    uint32_t tx_dma_ctrl;
    uint32_t res17;
    uint32_t tx_scb_burst_size;
    uint32_t res18[1004];

    // 0x6000
} BCM2711Genet_t;

#define BCM2711_GENET_RBUF_FLUSH_RESET BIT(1)

#define BCM2711_GENET_RGMII_OOB_CTRL_LINK BIT(4)
#define BCM2711_GENET_RGMII_OOB_CTRL_DISABLE BIT(5)
#define BCM2711_GENET_RGMII_OOB_CTRL_MODE_EN BIT(6)
#define BCM2711_GENET_RGMII_OOB_CTRL_ID_MODE_DISABLE BIT(16)

#define BCM2711_GENET_RBUF_CTRL_BAD_DIS BIT(2)
#define BCM2711_GENET_RBUF_CTRL_ALIGN_2B BIT(1)
#define BCM2711_GENET_RBUF_CTRL_64B_EN BIT(0)

#define BCM2711_GENET_TBUF_CTRL_EN BIT(0)

#define BCM2711_GENET_INTRL2_IRQ_TXDMA_DONE BIT(16)
#define BCM2711_GENET_INTRL2_IRQ_RXDMA_DONE BIT(13)

#define BCM2711_GENET_UMAC_CMD_LCL_LOOP_EN BIT(15)
#define BCM2711_GENET_UMAC_CMD_SW_RESET BIT(13)
#define BCM2711_GENET_UMAC_CMD_HD_EN BIT(10)
#define BCM2711_GENET_UMAC_CMD_CRC_FWD BIT(6)
#define BCM2711_GENET_UMAC_CMD_PROMISC BIT(4)
#define BCM2711_GENET_UMAC_CMD_SPEED_MASK (0x3 << 2)
#define BCM2711_GENET_UMAC_CMD_SPEED_10   (0 << 2)
#define BCM2711_GENET_UMAC_CMD_SPEED_100  (1 << 2)
#define BCM2711_GENET_UMAC_CMD_SPEED_1000 (2 << 2)
#define BCM2711_GENET_UMAC_CMD_RXEN BIT(1)
#define BCM2711_GENET_UMAC_CMD_TXEN BIT(0)

#define BCM2711_GENET_UMAC_MIB_CTRL_RESET_TX BIT(2)
#define BCM2711_GENET_UMAC_MIB_CTRL_RESET_RUNT BIT(1)
#define BCM2711_GENET_UMAC_MIB_CTRL_RESET_RX BIT(0)

#define BCM2711_GENET_MDIO_CMD_START_BUSY BIT(29)
#define BCM2711_GENET_MDIO_CMD_READ_FAILED BIT(28)
#define BCM2711_GENET_MDIO_CMD_READ BIT(27)
#define BCM2711_GENET_MDIO_CMD_WRITE BIT(26)
#define BCM2711_GENET_MDIO_CMD_PHY_SHIFT (21)
#define BCM2711_GENET_MDIO_CMD_PHY_MASK (0x1F)
#define BCM2711_GENET_MDIO_CMD_REG_SHIFT (16)
#define BCM2711_GENET_MDIO_CMD_REG_MASK (0x1F)
#define BCM2711_GENET_MDIO_CMD_VALUE_SHIFT (0)
#define BCM2711_GENET_MDIO_CMD_VALUE_MASK (0xFFFF)

#define BCM2711_GENET_DMA_STATUS_WRAP BIT(12)
#define BCM2711_GENET_DMA_STATUS_SOP BIT(13)
#define BCM2711_GENET_DMA_STATUS_EOP BIT(14)
#define BCM2711_GENET_DMA_STATUS_OWN BIT(15)
#define BCM2711_GENET_DMA_STATUS_LEN_MASK (0xfff)
#define BCM2711_GENET_DMA_STATUS_LEN_SHIFT (16)


#define BCM2711_GENET_DMA_STATUS_RX_OV BIT(0)
#define BCM2711_GENET_DMA_STATUS_RX_CRC_ERROR BIT(1)
#define BCM2711_GENET_DMA_STATUS_RX_RXER BIT(2)
#define BCM2711_GENET_DMA_STATUS_RX_NO BIT(3)
#define BCM2711_GENET_DMA_STATUS_RX_LG BIT(4)
#define BCM2711_GENET_DMA_STATUS_RX_MULTICAST BIT(5)
#define BCM2711_GENET_DMA_STATUS_RX_BROADCAST BIT(6)
#define BCM2711_GENET_DMA_STATUS_RX_FI_MASK (0x1f)
#define BCM2711_GENET_DMA_STATUS_RX_FI_SHIFT (7)
#define BCM2711_GENET_DMA_STATUS_RX_CHECK_V12 BIT(12)
#define BCM2711_GENET_DMA_STATUS_RX_CHECK_V3PLUS BIT(15)

#define BCM2711_GENET_DMA_STATUS_TX_DO_CSUM BIT(4)
#define BCM2711_GENET_DMA_STATUS_TX_OW_CRC BIT(5)
#define BCM2711_GENET_DMA_STATUS_TX_APPEND_CRC BIT(6)
#define BCM2711_GENET_DMA_STATUS_TX_UNDERRUN BIT(8)

#define BCM2711_GENET_RX_DMA_CTRL_RBUF_EN(rid) (BIT(rid+1))
#define BCM2711_GENET_RX_DMA_CTRL_EN BIT(0)

#define BCM2711_GENET_TX_DMA_CTRL_RBUF_EN(tid) (BIT(tid+1))
#define BCM2711_GENET_TX_DMA_CTRL_EN BIT(0)

// See IEEE 802.3 22.2.4
#define GENET_PHY_REG_BMCR (0x00)
#define GENET_PHY_REG_BMSR (0x01)
#define GENET_PHY_REG_IDR1 (0x02)
#define GENET_PHY_REG_IDR2 (0x03)
#define GENET_PHY_REG_ANAR (0x04)
#define GENET_PHY_REG_ANLPAR (0x05)
#define GENET_PHY_REG_ANER (0x06)
#define GENET_PHY_REG_ANNP (0x07)
#define GENET_PHY_REG_ANLPRNP (0x08)
#define GENET_PHY_REG_100T2CR (0x09)
#define GENET_PHY_REG_100T2SR (0x0a)
#define GENET_PHY_REG_PSECR (0x0b)
#define GENET_PHY_REG_PSESR (0x0c)
#define GENET_PHY_REG_MMDACR (0x0d)
#define GENET_PHY_REG_MMDAADR (0x0e)
#define GENET_PHY_REG_EXTSR (0x0f)
#define GENET_PHY_REG_AUXCTRL (0x18)
#define GENET_PHY_REG_SHADOW (0x1c)

#define GENET_PHY_BMCR_RESET BIT(15)
#define GENET_PHY_BMCR_LOOP BIT(14)
#define GENET_PHY_BMCR_SPEED0 BIT(13)
#define GENET_PHY_BMCR_AUTOEN BIT(12)
#define GENET_PHY_BMCR_PDOWN BIT(11)
#define GENET_PHY_BMCR_ISO BIT(10)
#define GENET_PHY_BMCR_STARTNEG BIT(9)
#define GENET_PHY_BMCR_FDX BIT(8)
#define GENET_PHY_BMCR_CTEST BIT(7)
#define GENET_PHY_BMCR_SPEED1 BIT(6)

#define GENET_PHY_BMSR_100T4 BIT(15)
#define GENET_PHY_BMSR_100TXFDX BIT(14)
#define GENET_PHY_BMSR_100TXHDX BIT(13)
#define GENET_PHY_BMSR_10TFDX BIT(12)
#define GENET_PHY_BMSR_10THDX BIT(11)
#define GENET_PHY_BMSR_100T2FDX BIT(10)
#define GENET_PHY_BMSR_100T2HDX BIT(9)
#define GENET_PHY_BMSR_EXTSTAT BIT(8)
#define GENET_PHY_BMSR_MFPS BIT(6)
#define GENET_PHY_BMSR_ACOMP BIT(5)
#define GENET_PHY_BMSR_RFAULT BIT(4)
#define GENET_PHY_BMSR_ANEG BIT(3)
#define GENET_PHY_BMSR_LINK BIT(2)
#define GENET_PHY_BMSR_JABBER BIT(1)
#define GENET_PHY_BMSR_EXTCAP BIT(0)

#define GENET_PHY_AUXCTRL_RGMII_SKEW_EN BIT(9)

#endif