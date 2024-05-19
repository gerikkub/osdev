
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
#include "kernel/cache.h"

#include "kernel/net/net.h"
#include "kernel/net/nic_ops.h"

#include "bcm2711_genet.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

typedef struct {
    volatile BCM2711Genet_t* mem;
    dt_node_t* dt_node;
    uint64_t intids[2];

    uint64_t phy_id;

    net_dev_t net_dev;

    uint64_t last_tx_cons_idx;
} bcm2711_genet_ctx_t;

static int32_t genet_read_mii(void* ctx, uint32_t reg) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    uint32_t stat = 0;

    uint32_t cmd = BCM2711_GENET_MDIO_CMD_READ |
                   (genet_ctx->phy_id & BCM2711_GENET_MDIO_CMD_PHY_MASK) << BCM2711_GENET_MDIO_CMD_PHY_SHIFT |
                   (reg & BCM2711_GENET_MDIO_CMD_REG_MASK) << BCM2711_GENET_MDIO_CMD_REG_SHIFT;

    genet_ctx->mem->mdio_cmd = cmd;

    stat = genet_ctx->mem->mdio_cmd;
    genet_ctx->mem->mdio_cmd = stat | BCM2711_GENET_MDIO_CMD_START_BUSY;

    uint32_t retries = 10;
    do {
        gtimer_busywait(10);
        stat = genet_ctx->mem->mdio_cmd;
    } while (stat & BCM2711_GENET_MDIO_CMD_START_BUSY && retries--);

    if (stat & BCM2711_GENET_MDIO_CMD_START_BUSY) {
        console_log(LOG_WARN, "BCM2711 Genet MII Read timeout %8x", stat);
        return -1;
    }

    if (stat & BCM2711_GENET_MDIO_CMD_READ_FAILED) {
        console_log(LOG_WARN, "BCM2711 Genet MII Read failed %8x", stat);
        return -1;
    }

    uint32_t ret = (stat >> BCM2711_GENET_MDIO_CMD_VALUE_SHIFT) & BCM2711_GENET_MDIO_CMD_VALUE_MASK;

    return ret;
}

static int32_t genet_write_mii(void* ctx, uint32_t reg, uint32_t val) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    uint32_t cmd = BCM2711_GENET_MDIO_CMD_WRITE |
                   BCM2711_GENET_MDIO_CMD_START_BUSY |
                   (genet_ctx->phy_id & BCM2711_GENET_MDIO_CMD_PHY_MASK) << BCM2711_GENET_MDIO_CMD_PHY_SHIFT |
                   (reg & BCM2711_GENET_MDIO_CMD_REG_MASK) << BCM2711_GENET_MDIO_CMD_REG_SHIFT |
                   (val & BCM2711_GENET_MDIO_CMD_VALUE_MASK) << BCM2711_GENET_MDIO_CMD_VALUE_SHIFT;

    genet_ctx->mem->mdio_cmd = cmd;
    
    uint32_t retries = 10;
    uint32_t stat;
    do {
        gtimer_busywait(10);
        stat = genet_ctx->mem->mdio_cmd;
    } while (stat & BCM2711_GENET_MDIO_CMD_START_BUSY && retries--);

    if (stat & BCM2711_GENET_MDIO_CMD_START_BUSY) {
        console_log(LOG_WARN, "BCM2711 Genet MII Write timeout %8x", stat);
        return -1;
    }

    return 0;
}

static void genet_reset(void* ctx) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    uint32_t stat;
    stat = genet_ctx->mem->rbuf_flush_ctrl;
    genet_ctx->mem->rbuf_flush_ctrl = stat |
                                      BCM2711_GENET_RBUF_FLUSH_RESET;
    gtimer_busywait(50);
    genet_ctx->mem->rbuf_flush_ctrl = stat;
    gtimer_busywait(50);

    genet_ctx->mem->rbuf_flush_ctrl = 0;
    gtimer_busywait(50);

    genet_ctx->mem->umac_cmd = 0;
    genet_ctx->mem->umac_cmd = BCM2711_GENET_UMAC_CMD_LCL_LOOP_EN |
                               BCM2711_GENET_UMAC_CMD_SW_RESET;
    gtimer_busywait(50);
    genet_ctx->mem->umac_cmd = 0;

    genet_ctx->mem->umac_mib_ctrl = BCM2711_GENET_UMAC_MIB_CTRL_RESET_TX |
                                    BCM2711_GENET_UMAC_MIB_CTRL_RESET_RX |
                                    BCM2711_GENET_UMAC_MIB_CTRL_RESET_RUNT;
    gtimer_busywait(50);
    genet_ctx->mem->umac_mib_ctrl = 0;

    genet_ctx->mem->umac_max_frame_len = 1536;

    stat = genet_ctx->mem->rbuf_ctrl;
    // stat |= BCM2711_GENET_RBUF_CTRL_ALIGN_2B;
    genet_ctx->mem->rbuf_ctrl = stat;

    genet_ctx->mem->tbuf_ctrl = BCM2711_GENET_TBUF_CTRL_EN;
}

static void genet_enable(void* ctx) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    uint32_t stat;

    stat = genet_ctx->mem->umac_cmd;
    stat |= BCM2711_GENET_UMAC_CMD_TXEN |
            BCM2711_GENET_UMAC_CMD_RXEN;
    genet_ctx->mem->umac_cmd = stat;
    
    genet_ctx->mem->intrl2_cpu_mask_clear = BCM2711_GENET_INTRL2_IRQ_TXDMA_DONE |
                                            BCM2711_GENET_INTRL2_IRQ_RXDMA_DONE;
}

static void genet_phy_setup(void* ctx) {
    // bcm2711_genet_ctx_t* genet_ctx = ctx;

    genet_write_mii(ctx, GENET_PHY_REG_BMCR, GENET_PHY_BMCR_RESET);

    int32_t stat;
    int32_t retries = 50;
    do {
        stat = genet_read_mii(ctx, GENET_PHY_REG_BMCR);
        gtimer_busywait(1000);
    } while (retries-- && stat > 0 && stat & GENET_PHY_BMCR_RESET);

    if (stat < 0 || stat & GENET_PHY_BMCR_RESET) {
        console_log(LOG_WARN, "Genet Unable to Reset PHY");
        return;
    }

    // Setup Rx Skew
    // int32_t auxctrl = 0x7 | (0x7 << 12);
    // genet_write_mii(ctx, GENET_PHY_REG_AUXCTRL, auxctrl);

    // auxctrl = genet_read_mii(ctx, GENET_PHY_REG_AUXCTRL);
    // ASSERT(auxctrl >= 0);

    // auxctrl = (auxctrl & 0x7FF8) |
    //           GENET_PHY_AUXCTRL_RGMII_SKEW_EN |
    //           (0x8000 | 0x7);
    // genet_write_mii(ctx, GENET_PHY_REG_AUXCTRL, auxctrl);

    // genet_write_mii(ctx, GENET_PHY_REG_SHADOW, 0x3 << 10);

    // int32_t shadow = genet_read_mii(ctx, GENET_PHY_REG_SHADOW);
    // shadow = (shadow & 0x7FF8) &
    //          (~(0x200));
    // genet_write_mii(ctx, GENET_PHY_REG_SHADOW, shadow);

    // int32_t gb100t2cr = genet_read_mii(ctx, GENET_PHY_REG_100T2CR);
    // int32_t gb100t2sr = genet_read_mii(ctx, GENET_PHY_REG_100T2SR);


    // int32_t bmcr = genet_read_mii(ctx, GENET_PHY_REG_BMCR);
    // genet_write_mii(ctx, GENET_PHY_REG_BMCR, bmcr | GENET_PHY_BMCR_STARTNEG);

    // int32_t bmsr;
    // do {
    //     gtimer_busywait(1000);
    //     bmsr = genet_read_mii(ctx, GENET_PHY_REG_BMSR);
    // } while(!(bmsr & GENET_PHY_BMSR_ACOMP));

}

// static void genet_print_dma_rx_ring(bcm2711_genet_ctx_t* genet_ctx, uint64_t rid) {

//     volatile BCM2711GenetRxDmaRing_t* r = &genet_ctx->mem->rx_ring[rid];

//     console_log(LOG_DEBUG, "GENET Rx Ring %d %16x", rid, r);
//     console_log(LOG_DEBUG, " Write Ptr Lo: %8x", r->write_ptr_lo);
//     console_log(LOG_DEBUG, " Write Ptr Hi: %8x", r->write_ptr_hi);
//     console_log(LOG_DEBUG, " Prod Index: %8x", r->prod_index);
//     console_log(LOG_DEBUG, " Cons Index: %8x", r->cons_index);
//     console_log(LOG_DEBUG, " Ring Size: %8x", r->ring_size);
//     console_log(LOG_DEBUG, " Start Addr Lo: %8x", r->start_addr_lo);
//     console_log(LOG_DEBUG, " Start Addr Hi: %8x", r->start_addr_hi);
//     console_log(LOG_DEBUG, " End Addr Lo: %8x", r->end_addr_lo);
//     console_log(LOG_DEBUG, " End Addr Hi: %8x", r->end_addr_hi);
//     console_log(LOG_DEBUG, " Done Threshold: %8x", r->done_thresh);
//     console_log(LOG_DEBUG, " Read Ptr Lo: %8x", r->read_ptr_lo);
//     console_log(LOG_DEBUG, " Read Ptr Hi: %8x", r->read_ptr_hi);
// }

// static void genet_print_dma_rx_desc(bcm2711_genet_ctx_t* genet_ctx, uint64_t did) {

//     volatile BCM2711GenetDmaDesc_t* d = &genet_ctx->mem->rx_desc[did];

//     console_log(LOG_DEBUG, "GENET Rx Desc[%d, %16x]: %8x %16x",
//                 did, d, d->length_status,
//                 (uint64_t)d->addr_lo | (((uint64_t)d->addr_hi) << 32));
// }

// static void genet_print_dma_tx_ring(bcm2711_genet_ctx_t* genet_ctx, uint64_t tid) {

//     volatile BCM2711GenetTxDmaRing_t* r = &genet_ctx->mem->tx_ring[tid];

//     console_log(LOG_DEBUG, "GENET Tx Ring %d %16x", tid, r);
//     console_log(LOG_DEBUG, " Write Ptr Lo: %8x", r->write_ptr_lo);
//     console_log(LOG_DEBUG, " Write Ptr Hi: %8x", r->write_ptr_hi);
//     console_log(LOG_DEBUG, " Prod Index: %8x", r->prod_index);
//     console_log(LOG_DEBUG, " Cons Index: %8x", r->cons_index);
//     console_log(LOG_DEBUG, " Ring Size: %8x", r->ring_size);
//     console_log(LOG_DEBUG, " Start Addr Lo: %8x", r->start_addr_lo);
//     console_log(LOG_DEBUG, " Start Addr Hi: %8x", r->start_addr_hi);
//     console_log(LOG_DEBUG, " End Addr Lo: %8x", r->end_addr_lo);
//     console_log(LOG_DEBUG, " End Addr Hi: %8x", r->end_addr_hi);
//     console_log(LOG_DEBUG, " Mbuf Done Threshold: %8x", r->mbuf_done_thresh);
//     console_log(LOG_DEBUG, " Flow Period: %8x", r->flow_period);
//     console_log(LOG_DEBUG, " Read Ptr Lo: %8x", r->read_ptr_lo);
//     console_log(LOG_DEBUG, " Read Ptr Hi: %8x", r->read_ptr_hi);
// }

// static void genet_print_dma_tx_desc(bcm2711_genet_ctx_t* genet_ctx, uint64_t did) {

//     volatile BCM2711GenetDmaDesc_t* d = &genet_ctx->mem->tx_desc[did];

//     console_log(LOG_DEBUG, "GENET Tx Desc[%d, %16x]: %8x %16x",
//                 did, d, d->length_status,
//                 (uint64_t)d->addr_lo | (((uint64_t)d->addr_hi) << 32));
// }

static void genet_init_rx_ring(bcm2711_genet_ctx_t* genet_ctx, uint64_t rid) {

    volatile BCM2711GenetRxDmaRing_t* r = &genet_ctx->mem->rx_ring[rid];

    r->write_ptr_lo = 0;
    r->write_ptr_hi = 0;
    r->prod_index = 0;
    r->cons_index = 0;
    r->ring_size = (256 << 16) | 2048;
    r->start_addr_lo = 0;
    r->start_addr_hi = 0;
    r->end_addr_lo = (GENET_DMA_DESC_COUNT * GENET_DMA_DESC_SIZE) / 4 - 1;
    r->end_addr_hi = 0;
    r->done_thresh = (5 << 16) | (GENET_DMA_DESC_COUNT >> 4);
    r->read_ptr_lo = 0;
    r->read_ptr_hi = 0;
}

static void genet_init_rx_descs(bcm2711_genet_ctx_t* genet_ctx) {

    volatile BCM2711GenetDmaDesc_t* desc = &genet_ctx->mem->rx_desc[0];

    for (uint64_t idx = 0; idx < GENET_DMA_DESC_COUNT; idx++) {
        uint64_t phy_addr = KSPACE_TO_PHY(kmalloc_phy(VMEM_PAGE_SIZE));

        desc[idx].addr_lo = phy_addr & 0xFFFFFFFF;
        desc[idx].addr_hi = (phy_addr >> 32) & 0xFFFFFFFF;

        // FreeBSD doesn't seem to write this field
        desc[idx].length_status = 0;

        memset(PHY_TO_KSPACE_PTR(phy_addr), 1, VMEM_PAGE_SIZE);
    }
}

static void genet_init_tx_ring(bcm2711_genet_ctx_t* genet_ctx, uint64_t tid) {

    volatile BCM2711GenetTxDmaRing_t* r = &genet_ctx->mem->tx_ring[tid];

    r->write_ptr_lo = 0;
    r->write_ptr_hi = 0;
    r->prod_index = 0;
    r->cons_index = 0;
    r->ring_size = (256 << 16) | 2048;
    r->start_addr_lo = 0;
    r->start_addr_hi = 0;
    r->end_addr_lo = (GENET_DMA_DESC_COUNT * GENET_DMA_DESC_SIZE) / 4 - 1;
    r->end_addr_hi = 0;
    r->mbuf_done_thresh = 1;
    r->flow_period = 0;
    r->read_ptr_lo = 0;
    r->read_ptr_hi = 0;
}

static void genet_init_tx_descs(bcm2711_genet_ctx_t* genet_ctx) {
    volatile BCM2711GenetDmaDesc_t* desc = &genet_ctx->mem->tx_desc[0];

    for (uint64_t idx = 0; idx < GENET_DMA_DESC_COUNT; idx++) {
        desc[idx].addr_lo = 0;
        desc[idx].addr_hi = 0;
        desc[idx].length_status = 0;
    }
}

static void genet_send_packet(bcm2711_genet_ctx_t* genet_ctx, uint64_t tid, void* buffer, uint64_t len) {

    console_log(LOG_DEBUG, "Genet Sending packet");
    dcache_clean_invalidate_range(buffer, len);

    volatile BCM2711GenetTxDmaRing_t* r = &genet_ctx->mem->tx_ring[tid];
    uint64_t next_idx = ((r->prod_index & 0xFFFF) + 1) % GENET_DMA_DESC_COUNT;
    volatile BCM2711GenetDmaDesc_t* desc = &genet_ctx->mem->tx_desc[r->prod_index & 0xFFFF];

    //for () {
        // volatile BCM2711GenetDmaDesc_t* desc = &genet_ctx->mem->tx_desc[i];

        desc->length_status = BCM2711_GENET_DMA_TX_DESC_STATUS_SOP |
                            BCM2711_GENET_DMA_TX_DESC_STATUS_EOP |
                            //   BCM2711_GENET_DMA_TX_DESC_STATUS_CRC |
                            (0x3F << 7) |
                            ((len & 0xFFF) << BCM2711_GENET_DMA_DESC_LENGTH_SHIFT);
        uintptr_t addr_phy = KSPACE_TO_PHY(buffer);
        desc->addr_lo = addr_phy & 0xFFFFFFFF;
        desc->addr_hi = (addr_phy >> 32) & 0xFFFFFFFF;

        MEM_DSB();
    //}

    r->prod_index = next_idx;

    char row_msg[16*3+1];

    console_log(LOG_DEBUG, "Sending Packet: %d", len);
    int idx = 0;
    for (int row = 0; idx < len; row+=16) {
        memset(row_msg, 0, sizeof(row_msg));
        for (int col = 0; col < 16 && idx < len; col++,idx++) {
            if (idx < len) {
                snprintf(&row_msg[col*3], 4, "%02x ", ((uint8_t*)buffer)[idx]);
            }
        }
        console_log(LOG_DEBUG, "%4x: %s", row, row_msg);
    }

    // int32_t bmsr = genet_read_mii(genet_ctx, GENET_PHY_REG_BMSR);
    // int32_t stat1000 = genet_read_mii(genet_ctx, GENET_PHY_REG_100T2SR);
    // int32_t ctrl1000 = genet_read_mii(genet_ctx, GENET_PHY_REG_100T2CR);
    // int32_t lpa = genet_read_mii(genet_ctx, GENET_PHY_REG_ANLPAR);
    // int32_t adv = genet_read_mii(genet_ctx, GENET_PHY_REG_ANAR);

    // console_log(LOG_DEBUG, "");
    // console_log(LOG_DEBUG, "BMSR: %4x", bmsr);
    // console_log(LOG_DEBUG, "1000 Status: %4x", stat1000);
    // console_log(LOG_DEBUG, "1000 Ctrl: %4x", ctrl1000);
    // console_log(LOG_DEBUG, "LPA: %4x", lpa);
    // console_log(LOG_DEBUG, "Advertise: %4x", adv);
    // console_log(LOG_DEBUG, "UMAC Cmd: %8x", genet_ctx->mem->umac_cmd);
    // console_log(LOG_DEBUG, "UMAC[0]: %8x", genet_ctx->mem->umac_mac0);
    // console_log(LOG_DEBUG, "UMAC[1]: %8x", genet_ctx->mem->umac_mac1);
    // console_log(LOG_DEBUG, "UMAC Frame Len: %8x", genet_ctx->mem->umac_max_frame_len);
    // console_log(LOG_DEBUG, "RBuf Ctrl: %8x", genet_ctx->mem->rbuf_ctrl);
    // console_log(LOG_DEBUG, "TBuf Ctrl: %8x", genet_ctx->mem->tbuf_ctrl);
    // console_log(LOG_DEBUG, "TBuf Size Ctrl: %8x", genet_ctx->mem->rbuf_tbuf_size_ctrl);
    // console_log(LOG_DEBUG, "Port Ctrl: %8x", genet_ctx->mem->port_ctrl);
    // console_log(LOG_DEBUG, "RGMII OOB Ctrl: %8x", genet_ctx->mem->rgmii_oob_ctrl);
    // console_log(LOG_DEBUG, "Tx DMA Ring Cfg: %8x %16x", genet_ctx->mem->tx_dma_ring_cfg, &genet_ctx->mem->tx_dma_ring_cfg);
    // console_log(LOG_DEBUG, "Tx DMA Ring Ctrl: %8x %16x", genet_ctx->mem->tx_dma_ctrl, &genet_ctx->mem->tx_dma_ctrl);
    // genet_print_dma_tx_ring(genet_ctx, 16);
    // genet_print_dma_tx_desc(genet_ctx, next_idx);
}

static void genet_reclaim_tx_packets(bcm2711_genet_ctx_t* genet_ctx, uint64_t tid) {
    volatile BCM2711GenetTxDmaRing_t* r = &genet_ctx->mem->tx_ring[tid];

    uint64_t cons_index = r->cons_index & 0xFFFF;

    uint64_t count = 0;

    while (genet_ctx->last_tx_cons_idx != cons_index)  {
        genet_ctx->last_tx_cons_idx = (genet_ctx->last_tx_cons_idx + 1) % GENET_DMA_DESC_COUNT;

        volatile BCM2711GenetDmaDesc_t* desc = &genet_ctx->mem->tx_desc[genet_ctx->last_tx_cons_idx];

        uintptr_t addr_phy = ((uintptr_t)desc->addr_hi) << 32 | (uintptr_t)desc->addr_lo;
        void* addr = PHY_TO_KSPACE_PTR(addr_phy);
        // TODO: Free memory
        //kfree_phy(addr);
        (void)addr;

        count++;
    }

    if (count > 0) {
        console_log(LOG_DEBUG, "Genet reclaimed %d Tx buffers", count);
    }
}

static void genet_irq_handler(uint32_t intid, void* ctx) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    uint32_t stat = genet_ctx->mem->intrl2_cpu_stat;
    uint32_t stat_mask = genet_ctx->mem->intrl2_cpu_mask_stat; 
    // console_log(LOG_DEBUG, " CPU Stat: %8x", stat);
    // console_log(LOG_DEBUG, " CPU Stat Mask: %8x", stat_mask);

    stat &= ~stat_mask;

    genet_ctx->mem->intrl2_cpu_clear = stat;

    // genet_print_dma_rx_ring(genet_ctx, 16);
    // for (uint64_t idx = 0; idx < GENET_DMA_DESC_COUNT; idx++) {
    //     genet_print_dma_rx_desc(genet_ctx, idx);
    // }
}

static int64_t genet_receive_packet(bcm2711_genet_ctx_t* genet_ctx, uint64_t did) {

    volatile BCM2711GenetDmaDesc_t* desc = &genet_ctx->mem->rx_desc[did];

    void* data = PHY_TO_KSPACE_PTR((uintptr_t)desc->addr_lo | (((uintptr_t)desc->addr_hi) << 16));

    uint64_t len = (desc->length_status >> BCM2711_GENET_DMA_DESC_LENGTH_SHIFT) &
                    BCM2711_GENET_DMA_DESC_LENGTH_MASK;
    uint64_t status = desc->length_status & 0xFFFF;

    if (status & BCM2711_GENET_DMA_RX_DESC_STATUS_RX_ERROR) {
        console_log(LOG_DEBUG, "Genet Receive Rx Error");
        return -1;
    }

    if (!(status & BCM2711_GENET_DMA_RX_DESC_STATUS_EOP &&
          status & BCM2711_GENET_DMA_RX_DESC_STATUS_SOP)) {
        // Don't handle fragmented packets
        //console_log(LOG_DEBUG, "Genet Receive Fragmented Packet");
        return -1;
    }
    dcache_clean_invalidate_range(data, len);

    net_packet_t* recv_packet = vmalloc(sizeof(net_packet_t));
    recv_packet->dev = &genet_ctx->net_dev;
    recv_packet->nic_pkt_ctx = (void*)desc;
    recv_packet->data = vmalloc(len);
    memcpy(recv_packet->data, data, len);
    recv_packet->len = len;

    net_recv_packet(recv_packet);

    return 0;
}

static void genet_irq_handler_a(uint32_t intid, void* ctx) {
    // console_log(LOG_DEBUG, "GENET IRQ A");
    genet_irq_handler(intid, ctx);
}

static void genet_irq_handler_b(uint32_t intid, void* ctx) {
    // console_log(LOG_DEBUG, "GENET IRQ B");
    genet_irq_handler(intid, ctx);
}

static net_send_buffer_t* genet_nic_get_buffer(net_dev_t* ctx, const int64_t size, const uint64_t flags) {
    net_send_buffer_t* send_buffer = vmalloc(sizeof(net_send_buffer_t));

    send_buffer->dev = ctx;
    send_buffer->data = kmalloc_phy(size);
    send_buffer->len = size;
    send_buffer->nic_buffer_ctx = ctx->nic_ctx;

    return send_buffer;
}

static void genet_nic_free_buffer(net_dev_t* ctx, net_send_buffer_t* buffer) {
    vfree(buffer->data);
    vfree(buffer);
}

static void genet_nic_send_buffer(net_dev_t* ctx, net_send_buffer_t* buffer) {

    genet_send_packet(ctx->nic_ctx, 16, buffer->data, buffer->len);
    vfree(buffer);

    genet_reclaim_tx_packets(ctx->nic_ctx, 16);
}

static void genet_nic_return_packet(net_packet_t* packet) {
    // Nothing to do. Rx will just re-use packets
}

static void genet_net_recv_thread(void* ctx) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    uint64_t cons_idx = 0;

    while(1) {
        task_wait_timer_in(100*1000);

        // genet_print_dma_rx_ring(genet_ctx, 16);
        uint64_t prod_idx = genet_ctx->mem->rx_ring[16].prod_index & 0xFFFF;

        // int32_t bmsr = genet_read_mii(ctx, GENET_PHY_REG_BMSR);
        // int32_t stat1000 = genet_read_mii(ctx, GENET_PHY_REG_100T2SR);
        // int32_t ctrl1000 = genet_read_mii(ctx, GENET_PHY_REG_100T2CR);
        // int32_t lpa = genet_read_mii(ctx, GENET_PHY_REG_ANLPAR);
        // int32_t adv = genet_read_mii(ctx, GENET_PHY_REG_ANAR);

        // console_log(LOG_DEBUG, "BMSR: %4x", bmsr);
        // console_log(LOG_DEBUG, "1000 Status: %4x", stat1000);
        // console_log(LOG_DEBUG, "1000 Ctrl: %4x", ctrl1000);
        // console_log(LOG_DEBUG, "LPA: %4x", lpa);
        // console_log(LOG_DEBUG, "Advertise: %4x", adv);
        // console_log(LOG_DEBUG, "UMAC Cmd: %8x", genet_ctx->mem->umac_cmd);
        // console_log(LOG_DEBUG, "UMAC[0]: %8x", genet_ctx->mem->umac_mac0);
        // console_log(LOG_DEBUG, "UMAC[1]: %8x", genet_ctx->mem->umac_mac1);
        // console_log(LOG_DEBUG, "UMAC Frame Len: %8x", genet_ctx->mem->umac_max_frame_len);
        // console_log(LOG_DEBUG, "RBuf Ctrl: %8x", genet_ctx->mem->rbuf_ctrl);
        // console_log(LOG_DEBUG, "Port Ctrl: %8x", genet_ctx->mem->port_ctrl);
        // console_log(LOG_DEBUG, "RGMII OOB Ctrl: %8x", genet_ctx->mem->rgmii_oob_ctrl);
        // console_log(LOG_DEBUG, "Rx DMA Ring Cfg: %8x %16x", genet_ctx->mem->rx_dma_ring_cfg, &genet_ctx->mem->rx_dma_ring_cfg);
        // console_log(LOG_DEBUG, "Rx DMA Ring Ctrl: %8x %16x", genet_ctx->mem->rx_dma_ctrl, &genet_ctx->mem->rx_dma_ctrl);

        while (prod_idx != cons_idx) {

            uint64_t phy_addr = (uint64_t)genet_ctx->mem->rx_desc[cons_idx].addr_lo | 
                                (uint64_t)genet_ctx->mem->rx_desc[cons_idx].addr_hi << 32;
            uint8_t* virt_addr = PHY_TO_KSPACE_PTR(phy_addr);
            for (uint64_t idx = 0; idx < 2048; idx += 32) {
                asm volatile("DC IVAC, %0"
                             :
                             : "r" (virt_addr + idx));
            }

            genet_receive_packet(genet_ctx, cons_idx);

            // genet_print_dma_rx_desc(genet_ctx, cons_idx);

            // for (uint64_t idx = 0; idx < 16; idx++) {
            //     console_log(LOG_DEBUG, "%2x: %2x %2x %2x %2x %2x %2x %2x %2x",
            //                 idx * 8,
            //                 virt_addr[idx*8 + 0], virt_addr[idx*8 + 1],
            //                 virt_addr[idx*8 + 2], virt_addr[idx*8 + 3],
            //                 virt_addr[idx*8 + 4], virt_addr[idx*8 + 5],
            //                 virt_addr[idx*8 + 6], virt_addr[idx*8 + 7]);
            // }
            cons_idx = (cons_idx + 1) % GENET_DMA_DESC_COUNT;

            genet_ctx->mem->rx_desc[cons_idx].length_status = 0;
            genet_ctx->mem->rx_ring[16].cons_index = cons_idx;
        }
    }
}

static int64_t genet_nic_ioctl(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    return -1;
}

static nic_ops_t s_genet_nic_ops = {
    .get_buffer = genet_nic_get_buffer,
    .send_buffer = genet_nic_send_buffer,
    .free_buffer = genet_nic_free_buffer,
    .return_packet = genet_nic_return_packet,
    .ioctl = genet_nic_ioctl
};

static void genet_late_init(void* ctx) {
    bcm2711_genet_ctx_t* genet_ctx = ctx;

    ASSERT(genet_ctx->dt_node->prop_ints->handler);
    ASSERT(genet_ctx->dt_node->prop_ints->handler->dtb_funcs);

    dt_node_t* intc_node = genet_ctx->dt_node->prop_ints->handler;
    intc_dtb_funcs_t* intc_dtb_funcs = intc_node->dtb_funcs;

    intc_dtb_funcs->get_intid_list(intc_node->dtb_ctx,
                                   genet_ctx->dt_node->prop_ints,
                                   &genet_ctx->intids[0]);

    intc_dtb_funcs->setup_intids(intc_node->dtb_ctx,
                                 genet_ctx->dt_node->prop_ints);

    interrupt_register_irq_handler(genet_ctx->intids[0], genet_irq_handler_a, ctx);
    interrupt_register_irq_handler(genet_ctx->intids[1], genet_irq_handler_b, ctx);
    interrupt_enable_irq(genet_ctx->intids[0]);
    // TODO: Not sure what the B handler is for. Seems to trigger constantly
    // interrupt_enable_irq(genet_ctx->intids[1]);

    // Initialization
    //  Reset
    //   - Set RBUF_FLUSH_RESET
    //   - Clear RBUF_FLSUH_RESET after delay
    //   - Write RBUF_FLUSH_CTRL 0
    //   - Write 0 UMAC CMD
    //   - Set UMAC LOOP_EN and SW_RESET
    //   - Write 0 UMAC CMD
    //   - Set MIB RESET RUNT, Rx and Tx
    //   - Write 0 MIB_CTRL
    //   - Write UMAC MAX_FRAME_LEN
    //   - Set RBUF_ALIGN 2B
    //   - Write 1 TBUF_SIZE_CTRL
    // Set Phy Mode
    //
    // Phy Init
    //
    // Set Mac Address
    //
    // Init DMA Rings
    //
    // Map DMA Rx Descriptors
    //
    // Enable TxRx


    genet_reset(genet_ctx);

    // Set PHY mode
    genet_ctx->mem->port_ctrl = 3;

    genet_enable(genet_ctx);

    int32_t bmcr = genet_read_mii(ctx, GENET_PHY_REG_BMCR);
    int32_t bmsr = genet_read_mii(ctx, GENET_PHY_REG_BMSR);
    int32_t gbsr = genet_read_mii(ctx, GENET_PHY_REG_100T2SR);
    int32_t gbcr = genet_read_mii(ctx, GENET_PHY_REG_100T2CR);
    int32_t lpa = genet_read_mii(ctx, GENET_PHY_REG_ANLPAR);
    int32_t adv = genet_read_mii(ctx, GENET_PHY_REG_ANAR);

    console_log(LOG_INFO, "Pre Setup");
    console_log(LOG_INFO, "GENET PHY BMCR %4x", bmcr);
    console_log(LOG_INFO, "GENET PHY BMSR %4x", bmsr);
    console_log(LOG_INFO, "GENET PHY GBSR %4x", gbsr);
    console_log(LOG_INFO, "GENET PHY GBCR %4x", gbcr);
    console_log(LOG_INFO, "GENET PHY LPA  %4x", lpa);
    console_log(LOG_INFO, "GENET PHY ADV  %4x", adv);

    // gbcr = 0;
    // genet_write_mii(ctx, GENET_PHY_REG_100T2CR, gbcr);
                        
    //genet_phy_setup(genet_ctx);

    bmcr = genet_read_mii(ctx, GENET_PHY_REG_BMCR);
    bmsr = genet_read_mii(ctx, GENET_PHY_REG_BMSR);
    gbsr = genet_read_mii(ctx, GENET_PHY_REG_100T2SR);
    gbcr = genet_read_mii(ctx, GENET_PHY_REG_100T2CR);
    lpa = genet_read_mii(ctx, GENET_PHY_REG_ANLPAR);
    adv = genet_read_mii(ctx, GENET_PHY_REG_ANAR);

    console_log(LOG_INFO, "Post Setup");
    console_log(LOG_INFO, "GENET PHY BMCR %4x", bmcr);
    console_log(LOG_INFO, "GENET PHY BMSR %4x", bmsr);
    console_log(LOG_INFO, "GENET PHY GBSR %4x", gbsr);
    console_log(LOG_INFO, "GENET PHY GBCR %4x", gbcr);
    console_log(LOG_INFO, "GENET PHY LPA  %4x", lpa);
    console_log(LOG_INFO, "GENET PHY ADV  %4x", adv);

    genet_write_mii(ctx, GENET_PHY_REG_BMCR, bmcr | (1 << 12));

    uint32_t rgmii = genet_ctx->mem->rgmii_oob_ctrl;
    // rgmii = (rgmii & ~(BIT(5)) & ~(BIT(16))) |
    rgmii &= ~BCM2711_GENET_RGMII_OOB_CTRL_DISABLE;
    rgmii |= BCM2711_GENET_RGMII_OOB_CTRL_ID_MODE_DISABLE |
             BCM2711_GENET_RGMII_OOB_CTRL_LINK |
             0;
             //BCM2711_GENET_RGMII_OOB_CTRL_MODE_EN;
    genet_ctx->mem->rgmii_oob_ctrl = rgmii;

    genet_ctx->mem->rbuf_tbuf_size_ctrl = 1;

    uint32_t umac = genet_ctx->mem->umac_cmd;
    //umac &= ~BCM2711_GENET_UMAC_CMD_HD_EN;
    umac |= BCM2711_GENET_UMAC_CMD_HD_EN;
    // umac |= BCM2711_GENET_UMAC_CMD_SPEED_1000;
    umac |= BCM2711_GENET_UMAC_CMD_SPEED_100;
    umac |= (BIT(8) | BIT(28));
    genet_ctx->mem->umac_cmd = umac;

    // Promisc mode
    // umac = umac | BIT(4);
    umac &= ~BCM2711_GENET_UMAC_CMD_PROMISC;
    genet_ctx->mem->umac_cmd = umac;

    genet_init_rx_ring(genet_ctx, 16);
    genet_init_rx_descs(genet_ctx);
    genet_init_tx_ring(genet_ctx, 16);
    genet_init_tx_descs(genet_ctx);

    uint32_t umac_lo = (uint32_t)genet_ctx->net_dev.mac.d[0] << 24 |
                       (uint32_t)genet_ctx->net_dev.mac.d[1] << 16 |
                       (uint32_t)genet_ctx->net_dev.mac.d[2] << 8 |
                       (uint32_t)genet_ctx->net_dev.mac.d[3];
    uint32_t umac_hi = (uint32_t)genet_ctx->net_dev.mac.d[4] << 8 |
                       (uint32_t)genet_ctx->net_dev.mac.d[5];
    genet_ctx->mem->umac_mac0 = umac_lo;
    genet_ctx->mem->umac_mac1 = umac_hi;

    genet_ctx->mem->rx_scb_burst_size = 8;
    genet_ctx->mem->rx_dma_ring_cfg = (1 << 16);

    // Enable DMA ring 16
    uint64_t stat = genet_ctx->mem->rx_dma_ctrl;
    stat |= BCM2711_GENET_RX_DMA_CTRL_RBUF_EN(16) |
            BCM2711_GENET_RX_DMA_CTRL_EN;
    genet_ctx->mem->rx_dma_ctrl = stat;

    genet_ctx->mem->tx_scb_burst_size = 8;
    genet_ctx->mem->tx_dma_ring_cfg = (1 << 16);
    genet_ctx->mem->tx_dma_ctrl = BCM2711_GENET_TX_DMA_CTRL_RBUF_EN(16) |
                                  BCM2711_GENET_TX_DMA_CTRL_EN;

    console_log(LOG_INFO, "GENET Tx DMA Desc %16x", &genet_ctx->mem->tx_desc[0]);
    console_log(LOG_INFO, "GENET Tx DMA Ring %16x", &genet_ctx->mem->tx_dma_ring_cfg);
    console_log(LOG_INFO, "GENET Tx DMA Ctrl %16x", &genet_ctx->mem->tx_dma_ctrl);
    console_log(LOG_INFO, "GENET Tx SCB %16x", &genet_ctx->mem->tx_scb_burst_size);
    
    create_kernel_task(8192, genet_net_recv_thread, genet_ctx, "genet-recv");

    genet_ctx->mem->intrl2_cpu_mask_clear = BCM2711_GENET_INTRL2_IRQ_RXDMA_DONE;

    stat = genet_ctx->mem->umac_cmd;
    stat |= BCM2711_GENET_UMAC_CMD_RXEN;
    stat |= BCM2711_GENET_UMAC_CMD_TXEN;
    genet_ctx->mem->umac_cmd = stat;

}

void bcm2711_genet_discover(void* ctx) {

    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    bcm2711_genet_ctx_t* genet_ctx = vmalloc(sizeof(bcm2711_genet_ctx_t));
    genet_ctx->dt_node = dt_node;

    dt_prop_reg_entry_t* reg_entries = dt_node->prop_reg->reg_entries;

    uintptr_t addr_bus;
    if (reg_entries->addr_size == 1) {
        addr_bus = reg_entries->addr_ptr[0];
    } else {
        addr_bus = ((uint64_t)reg_entries->addr_ptr[0] << 32) |
                    (uint64_t)reg_entries->addr_ptr[1];
    }

    uintptr_t mem_size;
    if (reg_entries->size_size == 1) {
        mem_size = reg_entries->size_ptr[0];
    } else {
        mem_size = ((uint64_t)reg_entries->size_ptr[0] << 32) |
                    (uint64_t)reg_entries->size_ptr[1];
    }

    bool addr_valid;
    uintptr_t genet_ctx_phy = dt_map_addr_to_phy(dt_node, addr_bus, &addr_valid);

    ASSERT(addr_valid);

    genet_ctx->mem = PHY_TO_KSPACE_PTR(genet_ctx_phy);

    memspace_map_device_kernel((void*)genet_ctx_phy,
                               (void*)genet_ctx->mem,
                               mem_size,
                               MEMSPACE_FLAG_PERM_KRW);
    memspace_update_kernel_vmem();

    console_log(LOG_INFO, "BCM2711 Genet Version %8x", genet_ctx->mem->sys_rev_ctrl);

    console_log(LOG_INFO, "intrl2: %16x", &genet_ctx->mem->intrl2_cpu_stat);
    console_log(LOG_INFO, "rbuf_ctrl: %16x", &genet_ctx->mem->rbuf_ctrl);
    console_log(LOG_INFO, "tbuf_ctrl: %16x", &genet_ctx->mem->tbuf_ctrl);
    console_log(LOG_INFO, "umac_cmd: %16x", &genet_ctx->mem->umac_cmd);
    console_log(LOG_INFO, "tx_flush: %16x", &genet_ctx->mem->tx_flush);
    console_log(LOG_INFO, "umac_mib_ctrl: %16x", &genet_ctx->mem->umac_mib_ctrl);
    console_log(LOG_INFO, "mdio_cmd: %16x", &genet_ctx->mem->mdio_cmd);
    console_log(LOG_INFO, "umac_mdf_ctrl: %16x", &genet_ctx->mem->umac_mdf_ctrl);

    uint32_t val = genet_ctx->mem->rbuf_flush_ctrl;
    console_log(LOG_INFO, "RBUF Flush Ctrl: %8x", val);

    uint64_t mac = (uint64_t)en_swap_32(genet_ctx->mem->umac_mac0) |
                   ((uint64_t)en_swap_32((genet_ctx->mem->umac_mac1 & 0xFFFF)) << 32);
    console_log(LOG_INFO, "Mac %2x:%2x:%2x:%2x:%2x:%2x",
                mac & 0xFF, (mac >> 8) & 0xFF, (mac >> 16) & 0xFF,
                (mac >> 24) & 0xFF, (mac >> 32) & 0xFF, (mac >> 40) & 0xFF);

    genet_ctx->phy_id = 1;

    genet_ctx->net_dev.ops = &s_genet_nic_ops;
    genet_ctx->net_dev.nic_ctx = genet_ctx;
    // TODO: Figure out why genet doesn't report mac address
    uint8_t mac_d[6] = {0xd8, 0x3a, 0xdd, 0x4c, 0xf0, 0xcf};
    memcpy(&genet_ctx->net_dev.mac.d, mac_d, sizeof(mac_d));
    genet_ctx->net_dev.name = "genet0";

    net_device_register(&genet_ctx->net_dev);


    driver_register_late_init(genet_late_init, genet_ctx);

}


void bcm2711_genet_register() {

    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "brcm,bcm2711-genet-v5"
        },
        .ctxfunc = bcm2711_genet_discover
    };
    register_driver(&reg);
}

// TODO: Uncomment to re-enable
//REGISTER_DRIVER(bcm2711_genet);