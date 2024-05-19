
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
#include "kernel/net/net.h"

#include "drivers/spi/spi.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

#include "enc28j60.h"

enum {
    ENC_REG_ETH,
    ENC_REG_MAC,
    ENC_REG_MII,
};

enum {
    // BANK 0 Registers
    ENC_REG_ERDPTL = 0,
    ENC_REG_ERDPTH,
    ENC_REG_EWRPTL,
    ENC_REG_EWRPTH,
    ENC_REG_ETXSTL,
    ENC_REG_ETXSTH,
    ENC_REG_ETXNDL,
    ENC_REG_ETXNDH,
    ENC_REG_ERXSTL,
    ENC_REG_ERXSTH,
    ENC_REG_ERXNDL,
    ENC_REG_ERXNDH,
    ENC_REG_ERXRDPTL,
    ENC_REG_ERXRDPTH,
    ENC_REG_ERXWRPTL,
    ENC_REG_ERXWRPTH,
    ENC_REG_EDMASTL,
    ENC_REG_EDMASTH,
    ENC_REG_EDMANDL,
    ENC_REG_EDMANDH,
    ENC_REG_EDMADSTL,
    ENC_REG_EDMADSTH,
    ENC_REG_EDMACSL,
    ENC_REG_EDMACSH,
    ENC_REG_BANK0_RES1,
    ENC_REG_BANK0_RES2,
    ENC_REG_BANK0_RES3,
    ENC_REG_EIE,
    ENC_REG_EIR,
    ENC_REG_ESTAT,
    ENC_REG_ECON2,
    ENC_REG_ECON1,

    // Bank 1 registers
    ENC_REG_EHT0,
    ENC_REG_EHT1,
    ENC_REG_EHT2,
    ENC_REG_EHT3,
    ENC_REG_EHT4,
    ENC_REG_EHT5,
    ENC_REG_EHT6,
    ENC_REG_EHT7,
    ENC_REG_EPMM0,
    ENC_REG_EPMM1,
    ENC_REG_EPMM2,
    ENC_REG_EPMM3,
    ENC_REG_EPMM4,
    ENC_REG_EPMM5,
    ENC_REG_EPMM6,
    ENC_REG_EPMM7,
    ENC_REG_EPMCSL,
    ENC_REG_EPMCSH,
    ENC_REG_BANK1_RES1,
    ENC_REG_BANK1_RES2,
    ENC_REG_EPMOL,
    ENC_REG_EPMOH,
    ENC_REG_BANK1_RES3,
    ENC_REG_BANK1_RES4,
    ENC_REG_ERXFCON,
    ENC_REG_EPKTCNT,
    ENC_REG_BANK1_RES5,
    ENC_REG_BANK1_EIE,
    ENC_REG_BANK1_EIR,
    ENC_REG_BANK1_ESTAT,
    ENC_REG_BANK1_ECON2,
    ENC_REG_BANK1_ECON1,

    // Bank 2 Registers
    ENC_REG_MACON1,
    ENC_REG_BANK2_RES1,
    ENC_REG_MACON3,
    ENC_REG_MACON4,
    ENC_REG_MABBIPG,
    ENC_REG_BANK2_RES2,
    ENC_REG_MAIPGL,
    ENC_REG_MAIPGH,
    ENC_REG_MACLCON1,
    ENC_REG_MACLCON2,
    ENC_REG_MAMXFLL,
    ENC_REG_MAMXFLH,
    ENC_REG_BANK2_RES3,
    ENC_REG_BANK2_RES4,
    ENC_REG_BANK2_RES5,
    ENC_REG_BANK2_RES6,
    ENC_REG_BANK2_RES7,
    ENC_REG_BANK2_RES8,
    ENC_REG_MICMD,
    ENC_REG_BANK2_RES9,
    ENC_REG_MIREGADR,
    ENC_REG_BANK2_RES10,
    ENC_REG_MIWRL,
    ENC_REG_MIWRH,
    ENC_REG_MIRDL,
    ENC_REG_MIRDH,
    ENC_REG_BANK2_RES11,
    ENC_REG_BANK2_EIE,
    ENC_REG_BANK2_EIR,
    ENC_REG_BANK2_ESTAT,
    ENC_REG_BANK2_ECON2,
    ENC_REG_BANK2_ECON1,

    // Bank 3 Registers
    ENC_REG_MAADR5,
    ENC_REG_MAADR6,
    ENC_REG_MAADR3,
    ENC_REG_MAADR4,
    ENC_REG_MAADR1,
    ENC_REG_MAADR2,
    ENC_REG_EBSTSD,
    ENC_REG_EBSTCON,
    ENC_REG_EBSTCSL,
    ENC_REG_EBSTCSH,
    ENC_REG_MISTAT,
    ENC_REG_BANK3_RES1,
    ENC_REG_BANK3_RES2,
    ENC_REG_BANK3_RES3,
    ENC_REG_BANK3_RES4,
    ENC_REG_BANK3_RES5,
    ENC_REG_BANK3_RES6,
    ENC_REG_BANK3_RES7,
    ENC_REG_EREVID,
    ENC_REG_BANK3_RES8,
    ENC_REG_BANK3_RES9,
    ENC_REG_ECOCON,
    ENC_REG_BANK3_RES10,
    ENC_REG_EFLOCON,
    ENC_REG_EPAUSL,
    ENC_REG_EPAUSH,
    ENC_REG_BANK3_RES11,
    ENC_REG_BANK3_EIE,
    ENC_REG_BANK3_EIR,
    ENC_REG_BANK3_ESTAT,
    ENC_REG_BANK3_ECON2,
    ENC_REG_BANK3_ECON1,
};

typedef struct {
    int64_t spi_fd;
    
    uint8_t reg_bank;

    net_dev_t nic;

    llist_head_t send_buffers;
} enc_ctx_t;

static void enc_cmd_spi_txn(enc_ctx_t* enc_ctx, uint8_t* write_buf, uint8_t* read_buf, uint64_t len) {
    fd_ctx_t* spi_fd_ctx = get_kernel_fd(enc_ctx->spi_fd);
    ASSERT(spi_fd_ctx != NULL);
    int64_t wrote = spi_fd_ctx->ops.write(spi_fd_ctx->ctx, write_buf, len, 0);
    ASSERT(wrote == len);

    uint64_t done;
    do {
        done = spi_fd_ctx->ops.read(spi_fd_ctx->ctx, read_buf, len, 0);
        ASSERT(done >= 0);
        // TODO: Can I use select and block inside the kernel?
        task_wait_timer_in(100);
    } while(!done);
}

static uint8_t enc_cmd_read_raw(enc_ctx_t* enc_ctx, uint8_t addr, int64_t reg_type) {
    ASSERT(addr < 0x20);

    uint8_t write_buf[3] = {0};
    uint8_t read_buf[3] = {0};

    write_buf[0] = addr;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, reg_type == ENC_REG_ETH ? 2 : 3);

    return read_buf[reg_type == ENC_REG_ETH ? 1 : 2];
}

static void enc_cmd_write_raw(enc_ctx_t* enc_ctx, uint8_t addr, uint8_t data) {
    ASSERT(addr < 0x20);

    uint8_t write_buf[2] = {0};
    uint8_t read_buf[2] = {0};

    write_buf[0] = 0x40 | addr;
    write_buf[1] = data;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, 2);
}

static void enc_cmd_bitset_raw(enc_ctx_t* enc_ctx, uint8_t addr, uint8_t data) {
    ASSERT(addr < 0x20);

    uint8_t write_buf[2] = {0};
    uint8_t read_buf[2] = {0};

    write_buf[0] = 0x80 | addr;
    write_buf[1] = data;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, 2);
}

static void enc_cmd_bitclr_raw(enc_ctx_t* enc_ctx, uint8_t addr, uint8_t data) {
    ASSERT(addr < 0x20);

    uint8_t write_buf[2] = {0};
    uint8_t read_buf[2] = {0};

    write_buf[0] = 0xA0 | addr;
    write_buf[1] = data;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, 2);
}

static void enc_cmd_switch_bank(enc_ctx_t* enc_ctx, uint8_t bank) {
    ASSERT(bank <= 3);
    if (bank != enc_ctx->reg_bank) {
        enc_cmd_bitclr_raw(enc_ctx, ENC_REG_ECON1, 0x3);
        enc_cmd_bitset_raw(enc_ctx, ENC_REG_ECON1, bank);
        enc_ctx->reg_bank = bank;
    }
}

static uint8_t enc_cmd_read(enc_ctx_t* enc_ctx, uint8_t addr, int64_t reg_type) {

    if ((addr & 0x1F) <= 0x1A) {
        uint8_t bank = (addr & 0x60) >> 5;
        enc_cmd_switch_bank(enc_ctx, bank);
    }

    return enc_cmd_read_raw(enc_ctx, addr & 0x1F, reg_type);
}


static void enc_cmd_write(enc_ctx_t* enc_ctx, uint8_t addr, uint8_t data) {

    if ((addr & 0x1F) <= 0x1A) {
        uint8_t bank = (addr & 0x60) >> 5;
        enc_cmd_switch_bank(enc_ctx, bank);
    }

    enc_cmd_write_raw(enc_ctx, addr & 0x1F, data);
}

static void enc_cmd_bitset(enc_ctx_t* enc_ctx, uint8_t addr, uint8_t data) {

    if ((addr & 0x1F) <= 0x1A) {
        uint8_t bank = (addr & 0x60) >> 5;
        enc_cmd_switch_bank(enc_ctx, bank);
    }

    uint8_t write_buf[2] = {0};
    uint8_t read_buf[2] = {0};

    write_buf[0] = 0x80 | addr;
    write_buf[1] = data;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, 2);
}

static void enc_cmd_bitclr(enc_ctx_t* enc_ctx, uint8_t addr, uint8_t data) {

    if ((addr & 0x1F) <= 0x1A) {
        uint8_t bank = (addr & 0x60) >> 5;
        enc_cmd_switch_bank(enc_ctx, bank);
    }

    uint8_t write_buf[2] = {0};
    uint8_t read_buf[2] = {0};

    write_buf[0] = 0xA0 | addr;
    write_buf[1] = data;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, 2);
}

static void enc_cmd_read_buffer(enc_ctx_t* enc_ctx, uint8_t* mem, int64_t len) {
    mem[0] = 0x3A;
    enc_cmd_spi_txn(enc_ctx, mem, mem, len);
}

static void enc_cmd_write_buffer(enc_ctx_t* enc_ctx, uint8_t* mem, int64_t len) {
    mem[0] = 0x7A;
    enc_cmd_spi_txn(enc_ctx, mem, mem, len);
}

static void enc_cmd_softreset(enc_ctx_t* enc_ctx) {
    uint8_t write_buf[1] = {0};
    uint8_t read_buf[1] = {0};

    write_buf[0] = 0xFF;

    enc_cmd_spi_txn(enc_ctx, write_buf, read_buf, 1);
}

void enc28j60_read_thread(void* ctx) {
    enc_ctx_t* enc_ctx = ctx;

    uint64_t read_off = 0x1000;

    console_log(LOG_INFO, "ENC Start Receive");
    while (true) {
        task_wait_timer_in(1000);

        while (true) {
            uint32_t eir = enc_cmd_read(enc_ctx, ENC_REG_EIR, ENC_REG_ETH);

            // If a new packet exists
            if (!(eir & BIT(6))) {
                break;
            }

            uint8_t recv_status[6+1];
            enc_cmd_write(enc_ctx, ENC_REG_ERDPTL, read_off);
            enc_cmd_write(enc_ctx, ENC_REG_ERDPTH, read_off >> 8);
            enc_cmd_read_buffer(enc_ctx, recv_status, 7);

            uint64_t next_packet_off = (recv_status[2] << 8) | recv_status[1];
            uint64_t packet_len = (recv_status[4] << 8) | recv_status[3];

            bool packet_ok = (recv_status[5] & BIT(7)) != 0;

            if (packet_ok) {
                uint8_t* packet_buffer = vmalloc(packet_len+1);
                enc_cmd_read_buffer(enc_ctx, packet_buffer, packet_len+1);

                net_packet_t pkt = {
                    .dev = &enc_ctx->nic,
                    .data = &packet_buffer[1],
                    .len = packet_len,
                    .nic_pkt_ctx = packet_buffer
                };

                net_recv_packet(&pkt);
            } else {
                console_log(LOG_INFO, "ENC Invalid Packet");
                console_log(LOG_INFO, "%2x %2x %2x %2x",
                            recv_status[6], recv_status[5], 
                            recv_status[4], recv_status[3]);

            }

            read_off = next_packet_off;

            enc_cmd_write(enc_ctx, ENC_REG_ERXRDPTL, read_off-1);
            enc_cmd_write(enc_ctx, ENC_REG_ERXRDPTH, (read_off-1) >> 8);

            enc_cmd_bitset(enc_ctx, ENC_REG_ECON2, BIT(6));
        }

        while (!llist_empty(enc_ctx->send_buffers)) {
            net_send_buffer_t* send_buffer = llist_at(enc_ctx->send_buffers, 0);
            llist_delete_ptr(enc_ctx->send_buffers, send_buffer);

            console_log(LOG_INFO, "ENC Sending Packet (%d)", send_buffer->len);
            enc_cmd_write(enc_ctx, ENC_REG_EWRPTL, 0);
            enc_cmd_write(enc_ctx, ENC_REG_EWRPTH, 0);
            enc_cmd_write(enc_ctx, ENC_REG_ETXSTL, 0);
            enc_cmd_write(enc_ctx, ENC_REG_ETXSTH, 0);
            uint8_t* pkt_buffer = send_buffer->nic_buffer_ctx;
            uint64_t write_len = send_buffer->len + 2 - 4;
            // Control byte
            pkt_buffer[1] = 0;

            enc_cmd_write_buffer(enc_ctx, pkt_buffer, write_len); // Discard CRC

            enc_cmd_write(enc_ctx, ENC_REG_ETXNDL, write_len-2);
            enc_cmd_write(enc_ctx, ENC_REG_ETXNDH, (write_len-2) >> 8);

            enc_cmd_bitset_raw(enc_ctx, ENC_REG_ECON1, BIT(3)); // TXRTS

            uint8_t tx_status;
            do {
                task_wait_timer_in(100);
                tx_status = enc_cmd_read(enc_ctx, ENC_REG_ECON1, ENC_REG_ETH);
            } while (tx_status & BIT(3)); //TXRTS

            uint8_t send_status[8];
            enc_cmd_write(enc_ctx, ENC_REG_ERDPTL, write_len-1);
            enc_cmd_write(enc_ctx, ENC_REG_ERDPTH, (write_len-1) >> 8);

            enc_cmd_read_buffer(enc_ctx, send_status, 8);
            console_log(LOG_INFO, "Send Status: %2x %2x %2x %2x %2x %2x %2x",
                        send_status[7], send_status[6], send_status[5],
                        send_status[4], send_status[3], send_status[2],
                        send_status[1]);
            // TODO: Do something with packet status ??

            vfree(send_buffer->nic_buffer_ctx);
            vfree(send_buffer);

            console_log(LOG_INFO, "ENC Sent Packet");
        }
    }
}

static net_send_buffer_t* enc_nic_get_buffer_fn(net_dev_t* ctx, const int64_t size, const uint64_t flags) {
    net_send_buffer_t* buf = vmalloc(sizeof(net_send_buffer_t));

    uint8_t* pkt_buffer = vmalloc(size+2);
    buf->dev = ctx;
    buf->data = &pkt_buffer[2];
    buf->len = size;
    buf->nic_buffer_ctx = pkt_buffer;

    return buf;
}

static void enc_nic_send_buffer_fn(net_dev_t* ctx, net_send_buffer_t* buffer) {
    enc_ctx_t* enc_ctx = ctx->nic_ctx;
    
    console_log(LOG_INFO, "ENC Request Send");
    llist_append_ptr(enc_ctx->send_buffers, buffer);
}

static void enc_nic_free_buffer_fn(net_dev_t* ctx, net_send_buffer_t* buffer) {
    //vfree(buffer->nic_buffer_ctx);
    //vfree(buffer);
}

static void enc_nic_return_packet_fn(net_packet_t* packet) {
    vfree(packet->nic_pkt_ctx);
}

static int64_t enc_nic_ioctl_fn(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    return -1;
}

static nic_ops_t s_enc_nic_ops = {
    .get_buffer = enc_nic_get_buffer_fn,
    .send_buffer = enc_nic_send_buffer_fn,
    .free_buffer = enc_nic_free_buffer_fn,
    .return_packet = enc_nic_return_packet_fn,
    .ioctl = enc_nic_ioctl_fn,
};

void enc28j60_discover(void* ctx) {
    enc28j60_discover_t* discover_ctx = ctx;
    (void)discover_ctx;

    enc_ctx_t* enc_ctx = vmalloc(sizeof(enc_ctx_t));
    enc_ctx->spi_fd = discover_ctx->spi_fd;

    console_log(LOG_INFO, "ENC28J60 found");

    enc_ctx->nic.ops = &s_enc_nic_ops;
    enc_ctx->nic.nic_ctx = enc_ctx;
    enc_ctx->nic.name = "enc28j60";
    memcpy(enc_ctx->nic.mac.d, "\xd8\x3a\xdd\x4c\xf0\x00", 6);

    enc_ctx->send_buffers = llist_create();

    enc_cmd_softreset(enc_ctx);

    uint8_t status;
    do {
        // Wait for oscillator startup
        status = enc_cmd_read_raw(enc_ctx, ENC_REG_ESTAT, ENC_REG_MAC);
    } while (!(status & BIT(0)));

    enc_cmd_write(enc_ctx, ENC_REG_ECON1, 0);
    enc_ctx->reg_bank = 0;

    // Enable MARXEN, TXPAUS, RXPAUS
    enc_cmd_write(enc_ctx, ENC_REG_MACON1, BIT(3) | // TXPAUS
                                           BIT(2) | // RXPAUS
                                           BIT(0)); // MARXEN
    // Configure PADCFG, TXCRCEN, FULDPX, FRMLNEN
    enc_cmd_write(enc_ctx, ENC_REG_MACON3, BIT(7) | BIT(6) | BIT(5) | // Pad short frames to 64 bytes and append CRC
                                           BIT(4) | // TXCRCEN
                                           BIT(1) | // FRMLNEN
                                           BIT(0)); // FULDPX
    // Configure DEFER
    enc_cmd_write(enc_ctx, ENC_REG_MACON4, BIT(6)); // DEFER
    // Configure Maximum Frame Len to 1518
    enc_cmd_write(enc_ctx, ENC_REG_MAMXFLL, 1518 & 0xFF);
    enc_cmd_write(enc_ctx, ENC_REG_MAMXFLH, 1518 >> 8);
    // Configure MABBIPG to 15h
    enc_cmd_write(enc_ctx, ENC_REG_MABBIPG, 0x15);
    // Configure MIPGL to 12h
    enc_cmd_write(enc_ctx, ENC_REG_MAIPGL, 0x12);
    enc_cmd_write(enc_ctx, ENC_REG_MAIPGH, 0);
    // Configure MAC Address to 12h
    enc_cmd_write(enc_ctx, ENC_REG_MAADR1, enc_ctx->nic.mac.d[0]);
    enc_cmd_write(enc_ctx, ENC_REG_MAADR2, enc_ctx->nic.mac.d[1]);
    enc_cmd_write(enc_ctx, ENC_REG_MAADR3, enc_ctx->nic.mac.d[2]);
    enc_cmd_write(enc_ctx, ENC_REG_MAADR4, enc_ctx->nic.mac.d[3]);
    enc_cmd_write(enc_ctx, ENC_REG_MAADR5, enc_ctx->nic.mac.d[4]);
    enc_cmd_write(enc_ctx, ENC_REG_MAADR6, enc_ctx->nic.mac.d[5]);

    //uint16_t read_buffer_size = 4096;
    uint16_t write_buffer_size = 0x1000;
    enc_cmd_write(enc_ctx, ENC_REG_ETXSTL, 0);
    enc_cmd_write(enc_ctx, ENC_REG_ETXSTH, 0);
    enc_cmd_write(enc_ctx, ENC_REG_ETXNDL, (write_buffer_size - 1) & 0xFF);
    enc_cmd_write(enc_ctx, ENC_REG_ETXNDH, (write_buffer_size - 1) >> 8);

    // Read Pointer Start
    enc_cmd_write(enc_ctx, ENC_REG_ERXSTL, write_buffer_size & 0xFF);
    enc_cmd_write(enc_ctx, ENC_REG_ERXSTH, write_buffer_size >> 8);
    enc_cmd_write(enc_ctx, ENC_REG_ERXNDL, 0x1FFF & 0xFF);
    enc_cmd_write(enc_ctx, ENC_REG_ERXNDH, 0x1FFF >> 8);

    enc_cmd_write(enc_ctx, ENC_REG_ERXRDPTL, 0x1FFF & 0xFF);
    enc_cmd_write(enc_ctx, ENC_REG_ERXRDPTH, 0x1FFF >> 8);

    // Enable packet reception
    enc_cmd_bitset(enc_ctx, ENC_REG_ECON1, BIT(2));

    net_device_register(&enc_ctx->nic);

    create_kernel_task(8192, enc28j60_read_thread, enc_ctx, "enc28j60-recv");
}

void enc28j60_register() {

    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_MANUAL,
        .manual = {
            .name = "enc28j60"
        },
        .ctxfunc = enc28j60_discover
    };
    register_driver(&reg);
}

REGISTER_DRIVER(enc28j60);
