
#include <stdint.h>
#include <stdlib/string.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"

#include "include/k_syscall.h"
#include "include/k_messages.h"
#include "include/k_modules.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"

#define VIRTIO_MMIO_MAGIC 0x74726976

typedef struct __attribute__((packed)) {
    uint32_t magic; // 0x74726976
    uint32_t version; // 0x2
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t features;
    uint32_t features_sel;
    uint32_t res0[2];
    uint32_t driver_features;
    uint32_t driver_features_sel;
    uint32_t res1[2];
    uint32_t queue_sel;
    uint32_t queue_num_max;
    uint32_t queue_num;
    uint32_t queue_ready;
    uint32_t res2[4];
    uint32_t queue_notify;
    uint32_t res3[3];
    uint32_t int_status;
    uint32_t int_ack;
    uint32_t res4[2];
    uint32_t status;
    uint32_t res5[3];
    uint32_t queue_desc_low;
    uint32_t queue_desc_high;
    uint32_t res6[2];
    uint32_t queue_avail_low;
    uint32_t queue_avail_high;
    uint32_t res7[2];
    uint32_t queue_used_low;
    uint32_t queue_used_high;
    uint32_t res8[21];
    uint32_t config_gen;
    uint8_t config[];
} virtio_mmio_struct_t;

void print_mmio(virtio_mmio_struct_t* device) {

    if (device->magic == VIRTIO_MMIO_MAGIC &&
        device->device_id != 0) {
        console_printf("Found MMIO: %u %u %u\n",
                        device->version,
                        device->device_id,
                        device->vendor_id);
        console_flush();
    }
}

void virtio_mmio_ctx(system_msg_memory_t* ctx_msg) {

    module_ctx_t* ctx = (module_ctx_t*)ctx_msg->ptr;
    if (ctx->startsel != MOD_STARTSEL_COMPAT &&
        ctx_msg->len >= sizeof(module_ctx_t)) {
        return;
    }
    char* name = (char*)ctx->ctx.dtb.name;

    console_printf("Got name: %s\n", name);
    console_flush();

    if (strncmp("virtio_mmio@", name, 12) != 0) {
        console_printf("Invalid name: %s\n", name);
        console_flush();
        return;
    }

    uint64_t name_len = strnlen(name, 64);
    char* loc_s = name + 12;
    uint64_t loc_len = name_len - 12;

    bool loc_valid;
    uint64_t loc = hextou64(loc_s, loc_len, &loc_valid);
    if (!loc_valid) {
        console_printf("Invalid location: %s\n", loc);
        console_flush();
        return;
    }

    virtio_mmio_struct_t* device;
    device = system_map_device(loc, 512);

    print_mmio(device);
}

static module_handlers_t s_handlers = {
    { //generic
        .info = NULL,
        .getinfo = NULL,
        .ioctl = NULL,
        .ctx = virtio_mmio_ctx
    }
};


void main(uint64_t my_tid, module_ctx_t* ctx) {

    console_write("MMIO Hello\n");
    console_flush();

    system_register_handler(s_handlers, MOD_CLASS_DISCOVERY);

    while(1) {
        system_recv_msg();
    }
}