
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/utils.h"
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
#include "kernel/select.h"
#include "kernel/interrupt/interrupt.h"

#include "include/k_ioctl_common.h"
#include "include/k_select.h"
#include "include/k_gpio.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

#include "bcm2711_gpio.h"

typedef struct {
    dt_node_t* dt_node;
    void* mem;
    int64_t num;
    int64_t offset;
    uint64_t intids[2];

    hashmap_ctx_t* listener_map;
} bcm2711_gpio_ctx_t;

typedef struct {
    bcm2711_gpio_ctx_t* gpio_ctx;
} bcm2711_gpio_fd_ctx_t;

typedef struct {
    bcm2711_gpio_ctx_t* gpio_ctx;
    uint64_t gpio_num;
    fd_ctx_t* fd_ctx;
} bcm2711_gpio_listener_ctx_t;

static int64_t bcm2711_gpio_configure(bcm2711_gpio_ctx_t* gpio_ctx, k_gpio_config_t* config) {
    if (config->gpio_num < gpio_ctx->offset ||
        config->gpio_num >= (gpio_ctx->offset + gpio_ctx->num)) {
        return -1;
    }

    uint32_t fsel_reg = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_FSET(config->gpio_num / 10));
    if (config->flags & GPIO_CONFIG_FLAG_AF_EN) {
        if (config->af > 5) {
            return -1;
        }

        fsel_reg &= ~((0x7) << ((config->gpio_num % 10)*3));
        uint32_t af_val = 0;
        switch (config->af) {
            case 0: af_val = 0x4; break;
            case 1: af_val = 0x5; break;
            case 2: af_val = 0x6; break;
            case 3: af_val = 0x7; break;
            case 4: af_val = 0x3; break;
            case 5: af_val = 0x2; break;
            default: ASSERT(false)
        };
        fsel_reg |= (af_val << ((config->gpio_num % 10)*3));
    } else {
        switch (config->flags & GPIO_CONFIG_FLAG_OUT_MASK) {
            case GPIO_CONFIG_FLAG_IN:
                fsel_reg &= ~((0x7) << ((config->gpio_num % 10)*3));
                break;
            case GPIO_CONFIG_FLAG_OUT_PP:
            case GPIO_CONFIG_FLAG_OUT_OD:
                fsel_reg &= ~((0x7) << ((config->gpio_num % 10)*3));
                fsel_reg |= (1) << ((config->gpio_num % 10)*3);
                break;
            default:
                return -1;
        }
    }

    uint32_t ren_reg = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_REN(config->gpio_num / 32));
    if (config->flags & GPIO_CONFIG_FLAG_EV_RISING) {
        ren_reg |= (1 << (config->gpio_num % 32));
    }

    uint32_t fen_reg = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_FEN(config->gpio_num / 32));
    if (config->flags & GPIO_CONFIG_FLAG_EV_FALLING) {
        fen_reg |= (1 << (config->gpio_num % 32));
    }

    uint32_t hen_reg = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_HEN(config->gpio_num / 32));
    if (config->flags & GPIO_CONFIG_FLAG_EV_HIGH) {
        hen_reg |= (1 << (config->gpio_num % 32));
    }

    uint32_t len_reg = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_LEN(config->gpio_num / 32));
    if (config->flags & GPIO_CONFIG_FLAG_EV_LOW) {
        len_reg |= (1 << (config->gpio_num % 32));
    }

    uint32_t pctrl_reg = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_PCTRL(config->gpio_num / 16));
    switch (config->flags & GPIO_CONFIG_FLAG_PULL_MASK) {
        case GPIO_CONFIG_FLAG_PULL_NONE:
            pctrl_reg &= ~((0x3) << ((config->gpio_num % 16)*2));
            break;
        case GPIO_CONFIG_FLAG_PULL_UP:
            pctrl_reg &= ~((0x3) << ((config->gpio_num % 16)*2));
            pctrl_reg |= (1) << ((config->gpio_num % 16)*2);
            break;
        case GPIO_CONFIG_FLAG_PULL_DOWN:
            pctrl_reg &= ~((0x3) << ((config->gpio_num % 16)*2));
            pctrl_reg |= (2) << ((config->gpio_num % 16)*2);
            break;
        default:
            return -1;
    }

    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_FSET(config->gpio_num / 10), fsel_reg);
    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_REN(config->gpio_num / 32), ren_reg);
    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_FEN(config->gpio_num / 32), fen_reg);
    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_HEN(config->gpio_num / 32), hen_reg);
    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_LEN(config->gpio_num / 32), len_reg);
    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_PCTRL(config->gpio_num / 16), pctrl_reg);

    return 0;
}

static int64_t bcm2711_gpio_set(bcm2711_gpio_ctx_t* gpio_ctx, k_gpio_level_t* level) {
    if (level->gpio_num < gpio_ctx->offset ||
        level->gpio_num >= (gpio_ctx->offset + gpio_ctx->num)) {
        return -1;
    }

    uint32_t pin = 1 << (level->gpio_num % 32);
    if (level->level) {
        WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_SET(level->gpio_num / 32), pin);
    } else {
        WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_CLR(level->gpio_num / 32), pin);
    }

    return 0;
}

static int64_t bcm2711_gpio_get(bcm2711_gpio_ctx_t* gpio_ctx, k_gpio_level_t* level) {
    if (level->gpio_num < gpio_ctx->offset ||
        level->gpio_num >= (gpio_ctx->offset + gpio_ctx->num)) {
        return -1;
    }
    
    uint32_t lev = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_LEV(level->gpio_num / 32));
    level->level = (lev & (1 << (level->gpio_num % 32))) != 0;

    return 0;
}

static int64_t bcm2711_gpio_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    bcm2711_gpio_fd_ctx_t* gpio_fd_ctx = vmalloc(sizeof(bcm2711_gpio_fd_ctx_t));

    gpio_fd_ctx->gpio_ctx = ctx;
    *ctx_out = gpio_fd_ctx;

    return 0;
}

static int64_t bcm2711_gpio_listener_close_op(void* ctx) {
    bcm2711_gpio_listener_ctx_t* l_ctx = ctx;

    hashmap_del(l_ctx->gpio_ctx->listener_map, &l_ctx->gpio_num);
    vfree(l_ctx);

    return 0;
}

static int64_t bcm2711_gpio_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    bcm2711_gpio_fd_ctx_t* gpio_fd_ctx = ctx;
    bcm2711_gpio_ctx_t* gpio_ctx = gpio_fd_ctx->gpio_ctx;

    switch (ioctl) {
        case GPIO_IOCTL_PROPERTIES:
            if (arg_count != 1) return -1;

            k_gpio_props_t* props = (k_gpio_props_t*)args[0];
            props->num = gpio_ctx->num;
            props->offset = gpio_ctx->offset;
            return 0;
        case GPIO_IOCTL_CONFIGURE:
            if (arg_count != 1) return -1;

            return bcm2711_gpio_configure(gpio_ctx, (k_gpio_config_t*)args[0]);
        case GPIO_IOCTL_SET:
            if (arg_count != 1) return -1;

            return bcm2711_gpio_set(gpio_ctx, (k_gpio_level_t*)args[0]);
        case GPIO_IOCTL_GET:
            if (arg_count != 1) return -1;

            return bcm2711_gpio_get(gpio_ctx, (k_gpio_level_t*)args[0]);
        case GPIO_IOCTL_LISTENER:
            if (arg_count != 1) return -1;

            k_gpio_listener_t* listener_info = get_kptr_for_ptr(args[0]);
            if (listener_info == NULL) return -1;

            if (listener_info->gpio_num < gpio_ctx->offset ||
                listener_info->gpio_num >= (gpio_ctx->offset + gpio_ctx->num)) return -1;
            
            // TODO: Support multiple listeners on a GPIO
            if (hashmap_contains(gpio_ctx->listener_map, &listener_info->gpio_num)) return -1;

            task_t* task = get_active_task();
            int64_t fd_num = find_open_fd(task);
            if (fd_num < 0) return -1;

            fd_ctx_t* fd_ctx = get_task_fd(fd_num, task);
            ASSERT(fd_ctx != NULL);

            bcm2711_gpio_listener_ctx_t* l_ctx = vmalloc(sizeof(bcm2711_gpio_listener_ctx_t));
            l_ctx->gpio_ctx = gpio_ctx;
            l_ctx->gpio_num = listener_info->gpio_num;
            l_ctx->fd_ctx = fd_ctx;

            fd_ctx->valid = true;
            fd_ctx->ctx = l_ctx;
            fd_ctx->ops.read = NULL;
            fd_ctx->ops.write = NULL;
            fd_ctx->ops.ioctl = NULL;
            fd_ctx->ops.close = bcm2711_gpio_listener_close_op;
            fd_ctx->ready = 0;
            fd_ctx->task = task;

            hashmap_add(gpio_ctx->listener_map, &l_ctx->gpio_num, fd_ctx);

            return fd_num;
    }

    return -1;
}

static int64_t bcm2711_gpio_close_op(void* ctx) {
    vfree(ctx);
    return 0;
}

static fd_ops_t s_gpio_file_ops = {
    .read = NULL,
    .write = NULL,
    .ioctl = bcm2711_gpio_ioctl_op,
    .close = bcm2711_gpio_close_op,
};

void bcm2711_gpio_irq_notify_listener(bcm2711_gpio_ctx_t* gpio_ctx, int64_t gpio_num) {

    fd_ctx_t* fd_ctx = hashmap_get(gpio_ctx->listener_map, &gpio_num);
    if (fd_ctx != NULL) {
        fd_ctx->ready |= FD_READY_GPIO_EVENT;

        select_task_wakeup(fd_ctx->task);
    }
}

void bcm2711_gpio_irq_handler(uint32_t intid, void* ctx) {
    bcm2711_gpio_ctx_t* gpio_ctx = ctx;

    uint32_t gpeds0, gpeds1;
    gpeds0 = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_EDS(0));
    gpeds1 = READ_MEM32(gpio_ctx->mem + BCM2711_GPIO_EDS(1));

    for (int i = 0; i < 32; i++) {
        if (gpeds0 & (1 << i)) {
            bcm2711_gpio_irq_notify_listener(gpio_ctx, i);
        }
    }

    for (int i = 32; i < 58; i++) {
        if (gpeds1 & (1 << (i-32))) {
            bcm2711_gpio_irq_notify_listener(gpio_ctx, i);
        }
    }

    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_EDS(0), gpeds0);
    WRITE_MEM32(gpio_ctx->mem + BCM2711_GPIO_EDS(1), gpeds1);
}

void bcm2711_gpio_irq_reg(void* ctx) {

    bcm2711_gpio_ctx_t* gpio_ctx = ctx;
    ASSERT(gpio_ctx->dt_node->prop_ints);
    // Note: There should be four IRQs according to the RPI4B
    // documentation, but only 2 show up in the device tree
    ASSERT(gpio_ctx->dt_node->prop_ints->num_ints == 2);
    ASSERT(gpio_ctx->dt_node->prop_ints->handler);
    ASSERT(gpio_ctx->dt_node->prop_ints->handler->dtb_funcs);


    dt_node_t* intc_node = gpio_ctx->dt_node->prop_ints->handler;
    intc_dtb_funcs_t* intc_dtb_funcs = intc_node->dtb_funcs;

    intc_dtb_funcs->get_intid_list(intc_node->dtb_ctx,
                                   gpio_ctx->dt_node->prop_ints,
                                   &gpio_ctx->intids[0]);

    intc_dtb_funcs->setup_intids(intc_node->dtb_ctx,
                                 gpio_ctx->dt_node->prop_ints);

    interrupt_register_irq_handler(gpio_ctx->intids[0], bcm2711_gpio_irq_handler, ctx);
    interrupt_register_irq_handler(gpio_ctx->intids[1], bcm2711_gpio_irq_handler, ctx);
    interrupt_enable_irq(gpio_ctx->intids[0]);
    interrupt_enable_irq(gpio_ctx->intids[1]);
}

void bcm2711_gpio_discover(void* ctx) {

    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node;

    bcm2711_gpio_ctx_t* gpio_ctx = vmalloc(sizeof(bcm2711_gpio_ctx_t));
    gpio_ctx->dt_node = dt_node;

    dt_prop_reg_entry_t* reg_entries = dt_node->prop_reg->reg_entries;

    ASSERT(reg_entries->addr_size == 1);
    uintptr_t addr_bus = reg_entries->addr_ptr[0];

    bool addr_valid;
    uintptr_t gpio_ctx_phy = dt_map_addr_to_phy(dt_node, addr_bus, &addr_valid);
    ASSERT(addr_valid);

    memspace_map_device_kernel((void*)gpio_ctx_phy,
                                PHY_TO_KSPACE_PTR(gpio_ctx_phy),
                                VMEM_PAGE_SIZE,
                                MEMSPACE_FLAG_PERM_KRW |
                                MEMSPACE_FLAG_IGNORE_DUPS);
    memspace_update_kernel_vmem();

    gpio_ctx->mem = PHY_TO_KSPACE_PTR(gpio_ctx_phy);
    gpio_ctx->num = 58;
    gpio_ctx->offset = 0;
    gpio_ctx->listener_map = uintmap_alloc(8);

    console_log(LOG_INFO, "BCM2711 GPIO @ %16x", gpio_ctx_phy);

    sys_device_register(&s_gpio_file_ops, bcm2711_gpio_open_op, gpio_ctx, "bcm2711_gpio");

    driver_register_late_init(bcm2711_gpio_irq_reg, gpio_ctx);
}

static void bcm2711_gpio_register() {

    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "brcm,bcm2711-gpio"
        },
        .ctxfunc = bcm2711_gpio_discover
    };
    register_driver(&reg);
}

REGISTER_DRIVER(bcm2711_gpio);