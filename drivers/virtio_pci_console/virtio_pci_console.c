
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lock/mutex.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/task.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"

#include "drivers/virtio_pci_console/virtio_pci_console.h"

typedef struct {
    virtio_virtq_ctx_t receiveq;
    virtio_virtq_ctx_t transmitq;
    virtio_virtq_buffer_t recv_buffer;
    virtio_virtq_shared_irq_ctx_t recv_irq_ctx;
} virtio_multiport_ctx_t;

typedef struct {
    pci_device_ctx_t pci_device;
    virtio_console_cfg_t* virtio_console_cfg;
    virtio_multiport_ctx_t* virtqueues;
    uint32_t num_virtqueues;
    virtio_virtq_ctx_t virtio_ctrl_receiveq;
    virtio_virtq_ctx_t virtio_ctrl_transmitq;
    virtio_virtq_shared_irq_ctx_t ctrl_irq_ctx;
    lock_t console_lock;
} virtio_console_ctx_t;

typedef struct {
    uint32_t port;
    bool open;
    virtio_console_ctx_t* console_ctx;
    uint8_t* recv_buf;
    int64_t recv_buf_len;
} virtio_console_dev_ctx_t;

typedef struct {
    virtio_console_dev_ctx_t* dev_ctx;
    fd_ctx_t* fd_ctx;
} virtio_console_open_ctx_t;

static void virtio_pci_console_device_irq_fn(uint32_t intid, void* ctx) {
    virtio_console_ctx_t* console_ctx = ctx;
    pci_interrupt_clear_pending(&console_ctx->pci_device, intid);

    if (console_ctx->ctrl_irq_ctx.intid == intid) {
        virtio_handle_irq(&console_ctx->ctrl_irq_ctx);
    } else {
        for (int64_t idx = 0; idx < console_ctx->num_virtqueues; idx++) {
            if (console_ctx->virtqueues[idx].recv_irq_ctx.intid == intid) {
                virtio_handle_irq(&console_ctx->virtqueues[idx].recv_irq_ctx);
            }
        }
    }

}

static void populate_receiveq_for_port(virtio_console_ctx_t* console_ctx, virtio_multiport_ctx_t* port_ctx) {

   virtio_virtq_ctx_t* receiveq = &port_ctx->receiveq;
   virtio_virtq_buffer_t* recv_buffer = &port_ctx->recv_buffer;
   bool get_ok;
   get_ok = virtio_get_buffer(receiveq, 2048, (uintptr_t*)&recv_buffer->ptr);
   ASSERT(get_ok);
   recv_buffer->len = 2048;

   virtio_virtq_send(receiveq, NULL, 0, recv_buffer, 1);
   virtio_virtq_notify(&console_ctx->pci_device, receiveq);
}

static void send_control_message(virtio_console_ctx_t* console_ctx, uint32_t port, uint16_t event, uint16_t value) {

    virtio_virtq_ctx_t* transmitq = &console_ctx->virtio_ctrl_transmitq;
    virtio_virtq_buffer_t send_buffer;
    bool get_ok;
    get_ok = virtio_get_buffer(transmitq, sizeof(virtio_console_ctrl_msg_t), (uintptr_t*)&send_buffer.ptr);
    ASSERT(get_ok);
    send_buffer.len = sizeof(virtio_console_ctrl_msg_t);

    virtio_console_ctrl_msg_t* ctrl_msg = (virtio_console_ctrl_msg_t*)send_buffer.ptr;
    ctrl_msg->port = port;
    ctrl_msg->event = event;
    ctrl_msg->value = value;

    virtio_virtq_send(transmitq, &send_buffer, 1, NULL, 0);
    virtio_virtq_notify(&console_ctx->pci_device, transmitq);
    virtio_poll_virtq(transmitq, true);

    virtio_return_buffer(transmitq, send_buffer.ptr);
}

static int64_t virtio_pci_console_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    virtio_console_dev_ctx_t* dev_ctx = ctx;

    virtio_console_open_ctx_t* open_ctx = vmalloc(sizeof(virtio_console_open_ctx_t));
    open_ctx->dev_ctx = dev_ctx;
    open_ctx->fd_ctx = fd_ctx;

    *ctx_out = open_ctx;

    if (fd_ctx != NULL) {
        fd_ctx->ready = FD_READY_ALL;
    }

    dev_ctx->open = true;

    if (dev_ctx->port > 0) {
        lock_acquire(&dev_ctx->console_ctx->console_lock, true);
        send_control_message(dev_ctx->console_ctx, dev_ctx->port, VIRTIO_CONSOLE_PORT_OPEN, 1);
        lock_release(&dev_ctx->console_ctx->console_lock);
    }

    return 0;
}

static int64_t virtio_pci_console_read_op(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    virtio_console_dev_ctx_t* dev_ctx = ((virtio_console_open_ctx_t*)ctx)->dev_ctx;

    // Copy out of the temporary buffer if possible
    if (dev_ctx->recv_buf != NULL) {
        ASSERT(dev_ctx->recv_buf_len > 0);
        const int64_t recv_len = size > dev_ctx->recv_buf_len ? dev_ctx->recv_buf_len : size;

        memcpy(buffer, dev_ctx->recv_buf, recv_len);
        if (recv_len < dev_ctx->recv_buf_len) {
            memmove(dev_ctx->recv_buf, &dev_ctx->recv_buf[recv_len], dev_ctx->recv_buf_len - recv_len);
            dev_ctx->recv_buf_len -= recv_len;
        } else {
            vfree(dev_ctx->recv_buf);
            dev_ctx->recv_buf = NULL;
            dev_ctx->recv_buf_len = 0;
        }

        return recv_len;
    }

    // Fetch new data from the virtq
    virtio_console_ctx_t* console_ctx = dev_ctx->console_ctx;
    lock_acquire(&console_ctx->console_lock, true);

    const uint32_t port = dev_ctx->port;
    ASSERT(port < console_ctx->num_virtqueues);

    virtio_virtq_ctx_t* receiveq = &console_ctx->virtqueues[port].receiveq;
    virtio_virtq_buffer_t* recv_buffer = &console_ctx->virtqueues[port].recv_buffer;

    virtio_poll_virtq_irq(receiveq, &console_ctx->virtqueues[port].recv_irq_ctx);
    const int64_t recv_len = virtio_get_used_elem(receiveq, 0);
    ASSERT(recv_len > 0);

    if (recv_len <= size) {
        // All data can fit in the requested read buffer
        memcpy(buffer, recv_buffer->ptr, recv_len);
        virtio_return_buffer(receiveq, recv_buffer->ptr);

        populate_receiveq_for_port(console_ctx, &console_ctx->virtqueues[port]);

        lock_release(&console_ctx->console_lock);
        return recv_len;
    } else {
        // Need a temporary buffer to receive all data
        memcpy(buffer, recv_buffer->ptr, size);

        const int64_t remaining = recv_len - size;
        dev_ctx->recv_buf = vmalloc(remaining);
        memcpy(dev_ctx->recv_buf, recv_buffer->ptr+size, remaining);
        dev_ctx->recv_buf_len = remaining;

        virtio_return_buffer(receiveq, recv_buffer->ptr);

        populate_receiveq_for_port(console_ctx, &console_ctx->virtqueues[port]);

        lock_release(&console_ctx->console_lock);
        return size;
    }
}

static int64_t virtio_pci_console_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    if (size == 0) {
        return 0;
    }
    virtio_console_dev_ctx_t* dev_ctx = ((virtio_console_open_ctx_t*)ctx)->dev_ctx;

    virtio_console_ctx_t* console_ctx = dev_ctx->console_ctx;
    lock_acquire(&console_ctx->console_lock, true);
    const uint32_t port = dev_ctx->port;
    ASSERT(port < console_ctx->num_virtqueues);

    virtio_virtq_ctx_t* transmitq = &console_ctx->virtqueues[port].transmitq;

    virtio_virtq_buffer_t write_buffer;
    bool get_ok;
    get_ok = virtio_get_buffer(transmitq, size, (uintptr_t*)&write_buffer.ptr);
    ASSERT(get_ok);
    write_buffer.len = size;
    memcpy(write_buffer.ptr, buffer, size);

    virtio_virtq_send(transmitq, &write_buffer, 1, NULL, 0);
    virtio_virtq_notify(&console_ctx->pci_device, transmitq);
    virtio_poll_virtq(transmitq, true);

    virtio_return_buffer(transmitq, write_buffer.ptr);

    lock_release(&console_ctx->console_lock);

    return size;
}

static int64_t virtio_pci_console_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    return -1;
}

static int64_t virtio_pci_console_close_op(void* ctx) {
    virtio_console_dev_ctx_t* dev_ctx = ((virtio_console_open_ctx_t*)ctx)->dev_ctx;
    dev_ctx->open = false;

    if (dev_ctx->port > 0) {
        lock_acquire(&dev_ctx->console_ctx->console_lock, true);
        send_control_message(dev_ctx->console_ctx, dev_ctx->port, VIRTIO_CONSOLE_PORT_OPEN, 0);
        lock_release(&dev_ctx->console_ctx->console_lock);
    }

    vfree(ctx);

    return 0;
}

static fd_ops_t s_virtio_pci_console_file_ops = {
    .read = virtio_pci_console_read_op,
    .write = virtio_pci_console_write_op,
    .ioctl = virtio_pci_console_ioctl_op,
    .close = virtio_pci_console_close_op,
};

static void handle_ctrl_message_add(virtio_console_ctx_t* console_ctx, virtio_console_ctrl_msg_t* msg, uint64_t len) {

    char name[] = "con00";
    virtio_console_dev_ctx_t* dev_ctx = vmalloc(sizeof(virtio_console_dev_ctx_t));
    dev_ctx->port = msg->port;
    dev_ctx->open = false;
    dev_ctx->console_ctx = console_ctx;
    dev_ctx->recv_buf = NULL;
    name[3] = (dev_ctx->port / 10) + '0';
    name[4] = (dev_ctx->port % 10) + '0';
    sys_device_register(&s_virtio_pci_console_file_ops, virtio_pci_console_open_op, dev_ctx, name);

    pci_enable_vector(&console_ctx->pci_device, console_ctx->virtqueues[msg->port].recv_irq_ctx.intid);

    send_control_message(console_ctx, msg->port, VIRTIO_CONSOLE_PORT_READY, 1);
}

static void handle_ctrl_message(virtio_console_ctx_t* console_ctx, virtio_console_ctrl_msg_t* msg, uint64_t len) {
    ASSERT(len >= sizeof(virtio_console_ctrl_msg_t));

    console_log(LOG_DEBUG, "Got console event %d\n", msg->event);

    ASSERT(msg->event < VIRTIO_CONSOLE_Max);

    lock_acquire(&console_ctx->console_lock, true);

    switch (msg->event) {
        case VIRTIO_CONSOLE_DEVICE_ADD:
            handle_ctrl_message_add(console_ctx, msg, len);
            break;
    }

    lock_release(&console_ctx->console_lock);
}

static void virtio_pci_control_monitor_thread(void* ctx) {

    virtio_console_ctx_t* console_ctx = ctx;

    while (1) {

        virtio_virtq_buffer_t recv_buffer;
        virtio_virtq_ctx_t* receiveq = &console_ctx->virtio_ctrl_receiveq;
        bool get_ok;
        get_ok = virtio_get_buffer(receiveq, 2048, (uintptr_t*)&recv_buffer.ptr);
        recv_buffer.len = 2048;
        ASSERT(get_ok);

        virtio_virtq_send(receiveq, NULL, 0, &recv_buffer, 1);
        virtio_virtq_notify(&console_ctx->pci_device, receiveq);

        virtio_poll_virtq_irq(receiveq, &console_ctx->ctrl_irq_ctx);

        handle_ctrl_message(console_ctx, (virtio_console_ctrl_msg_t*)recv_buffer.ptr, recv_buffer.len);

        virtio_return_buffer(receiveq, recv_buffer.ptr);
    }

}


static void init_console_device(virtio_console_ctx_t* console_ctx) {

    pci_device_ctx_t* pci_ctx = &console_ctx->pci_device;
    pci_virtio_common_cfg_t* common_cfg = NULL;
    pci_virtio_capability_t* cap_ptr = NULL;

    cap_ptr = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_COMMON_CFG);
    ASSERT(cap_ptr);

    common_cfg = GET_CAP_PTR(pci_ctx, cap_ptr);

    uint64_t features_req = (1UL << VIRTIO_CONSOLE_F_MULTIPORT) |
                            (1UL << VIRTIO_CONSOLE_F_EMERG_WRITE) |
                            (1UL << VIRTIO_F_VERSION_1);
    bool status = virtio_init_with_features(pci_ctx, features_req);
    ASSERT(status);


    uint32_t port0_rq_intid;
    port0_rq_intid = pci_register_interrupt_handler(
                                        &console_ctx->pci_device,
                                        virtio_pci_console_device_irq_fn,
                                        console_ctx);

    pci_msix_vector_ctx_t* port0_rq_msix_item = pci_get_msix_entry(&console_ctx->pci_device, port0_rq_intid);
    ASSERT(port0_rq_msix_item != NULL);

    pci_virtio_capability_t* console_cfg_cap;
    console_cfg_cap = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG);
    ASSERT(console_cfg_cap);
    console_ctx->virtio_console_cfg = GET_CAP_PTR(pci_ctx, console_cfg_cap);

    console_ctx->num_virtqueues = console_ctx->virtio_console_cfg->max_nr_ports;
    console_ctx->virtqueues = vmalloc(console_ctx->num_virtqueues * sizeof(virtio_multiport_ctx_t));

    mutex_init(&console_ctx->console_lock, 8);

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_CONSOLE_RECEIVEQ_0,
                       1, 4096,
                       &console_ctx->virtqueues[0].receiveq,
                       port0_rq_msix_item->entry_idx);

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_CONSOLE_TRANSMITQ_0,
                       4, 4096,
                       &console_ctx->virtqueues[0].transmitq,
                       VIRTIO_MSI_NO_VECTOR);

    console_ctx->virtqueues[0].recv_irq_ctx.wait_queue = llist_create();
    console_ctx->virtqueues[0].recv_irq_ctx.intid = port0_rq_intid;

    for (int idx = 1; idx < console_ctx->num_virtqueues; idx++) {

        uint32_t portn_rq_intid;
        portn_rq_intid = pci_register_interrupt_handler(
                                            &console_ctx->pci_device,
                                            virtio_pci_console_device_irq_fn,
                                            console_ctx);

        pci_msix_vector_ctx_t* portn_rq_msix_item = pci_get_msix_entry(&console_ctx->pci_device, portn_rq_intid);
        ASSERT(portn_rq_msix_item != NULL);

        virtio_alloc_queue(common_cfg,
                            VIRTIO_QUEUE_CONSOLE_RECEIVEQ_N(idx),
                            1, 4096,
                            &console_ctx->virtqueues[idx].receiveq,
                            portn_rq_msix_item->entry_idx);

        virtio_alloc_queue(common_cfg,
                            VIRTIO_QUEUE_CONSOLE_TRANSMITQ_N(idx),
                            4, 4096,
                            &console_ctx->virtqueues[idx].transmitq,
                            VIRTIO_MSI_NO_VECTOR);
                        
        console_ctx->virtqueues[idx].recv_irq_ctx.wait_queue = llist_create();
        console_ctx->virtqueues[idx].recv_irq_ctx.intid = portn_rq_intid;
    }

    uint32_t ctrl_intid;
    ctrl_intid = pci_register_interrupt_handler(
                                        &console_ctx->pci_device,
                                        virtio_pci_console_device_irq_fn,
                                        console_ctx);

    pci_msix_vector_ctx_t* msix_item = pci_get_msix_entry(&console_ctx->pci_device, ctrl_intid);
    ASSERT(msix_item != NULL);

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_CONSOLE_CTRL_RECEIVEQ,
                       1, 4096,
                       &console_ctx->virtio_ctrl_receiveq,
                       msix_item->entry_idx);

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_CONSOLE_CTRL_TRANSMITQ,
                       1, 4096,
                       &console_ctx->virtio_ctrl_transmitq,
                       VIRTIO_MSI_NO_VECTOR);

    console_ctx->ctrl_irq_ctx.wait_queue = llist_create();
    console_ctx->ctrl_irq_ctx.intid = ctrl_intid;



    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER_OK);

    pci_enable_vector(&console_ctx->pci_device, console_ctx->ctrl_irq_ctx.intid);
    pci_enable_vector(&console_ctx->pci_device, console_ctx->virtqueues[0].recv_irq_ctx.intid);
    pci_enable_interrupts(&console_ctx->pci_device);

    send_control_message(console_ctx, 0, VIRTIO_CONSOLE_DEVICE_READY, 1);

    populate_receiveq_for_port(console_ctx, &console_ctx->virtqueues[0]);

    send_control_message(console_ctx, 0, VIRTIO_CONSOLE_PORT_OPEN, 1);

    // Setup a kernel thread to monitor for receiveq messages
    create_kernel_task(8192, virtio_pci_control_monitor_thread, console_ctx, "virtio-con-ctrlq");

}

static void virtio_console_late_init(void* ctx) {
    discovery_pci_ctx_t* pci_ctx = ctx;

    virtio_console_ctx_t* console_ctx = vmalloc(sizeof(virtio_console_ctx_t));

    pci_alloc_device_from_context(&console_ctx->pci_device, pci_ctx);

    init_console_device(console_ctx);

    virtio_console_dev_ctx_t* dev_ctx = vmalloc(sizeof(virtio_console_dev_ctx_t));
    virtio_console_open_ctx_t* open_ctx = vmalloc(sizeof(virtio_console_open_ctx_t));
    dev_ctx->port = 0;
    dev_ctx->open = false;
    dev_ctx->console_ctx = console_ctx;
    dev_ctx->recv_buf = NULL;
    open_ctx->dev_ctx = dev_ctx;
    open_ctx->fd_ctx = NULL;
    console_add_driver(&s_virtio_pci_console_file_ops, open_ctx);

    sys_device_register(&s_virtio_pci_console_file_ops, virtio_pci_console_open_op, dev_ctx, "con0");
    console_log(LOG_INFO, "Hello from serial driver!");

    vfree(pci_ctx);
}

static void virtio_pci_console_ctx(void* ctx) {

    console_log(LOG_DEBUG, "virtio-pci-blk got ctx");
    console_flush();

    discovery_pci_ctx_t* pci_ctx = ctx;
    discovery_pci_ctx_t* pci_ctx_copy = vmalloc(sizeof(discovery_pci_ctx_t));
    *pci_ctx_copy = *pci_ctx;

    driver_register_late_init(virtio_console_late_init, pci_ctx_copy);
}


static discovery_register_t s_virtio_pci_console_register = {
    .type = DRIVER_DISCOVERY_PCI,
    .pci = {
        .vendor_id = 0x1AF4,
        .device_id = 0x1043,
    },
    .ctxfunc = virtio_pci_console_ctx
};

void virtio_pci_console_register(void) {

    register_driver(&s_virtio_pci_console_register);
}

REGISTER_DRIVER(virtio_pci_console);
