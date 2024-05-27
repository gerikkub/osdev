
#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/lib/libdtb.h"

#include "stdlib/bitutils.h"

#define BEGIN_CRITICAL(x) \
    do { \
    READ_SYS_REG(DAIF, x);  \
    volatile uint64_t _val = BIT(8) | /* Serror Mask */ \
                             BIT(7) | /* IRQ Mask */ \
                             BIT(6);  /* FIQ Mask */ \
    WRITE_SYS_REG(DAIF, _val); \
    (void)_val; \
    } while(0)

#define END_CRITICAL(x) \
    do { \
    WRITE_SYS_REG(DAIF, x); \
    } while(0)

#define ENABLE_IRQ() \
    do { \
    volatile uint64_t _val; \
    READ_SYS_REG(DAIF, _val);  \
    _val &= ~(BIT(7) | BIT(6)); /* IRQ Mask */ \
    WRITE_SYS_REG(DAIF, _val); \
    (void)_val; \
    } while(0)

#define DISABLE_IRQ() \
    do { \
    volatile uint64_t _val; \
    READ_SYS_REG(DAIF, _val);  \
    _val |= BIT(7) | BIT(6); /* IRQ Mask */ \
    WRITE_SYS_REG(DAIF, _val); \
    (void)_val; \
    } while(0)

typedef enum {
    INTERRUPT_TRIGGER_LEVEL = 0,
    INTERRUPT_TRIGGER_EDGE = 1
} interrupt_trigger_type_t;

typedef void (*irq_handler)(uint32_t intid, void* ctx);

typedef void (*intc_enable_fn)(void* ctx);
typedef void (*intc_disable_fn)(void* ctx);
typedef void (*intc_enable_irq_fn)(void* ctx, uint64_t irq);
typedef void (*intc_disable_irq_fn)(void* ctx, uint64_t irq);
typedef void (*intc_irq_trigger_fn)(void* ctx, uint64_t irq, interrupt_trigger_type_t type);
typedef void (*intc_get_msi_fn)(void* ctx, uint64_t* intid_out, uint64_t* data_out, uintptr_t* addr_out);
typedef void (*intc_irq_handler_fn)(void* ctx);

typedef struct {
    intc_enable_fn enable;
    intc_disable_fn disable;
    intc_enable_irq_fn enable_irq;
    intc_disable_irq_fn disable_irq;
    intc_irq_trigger_fn set_irq_trigger;
    intc_get_msi_fn get_msi;
    intc_irq_handler_fn irq_handler;
} intc_funcs_t;

typedef void (*intc_dtb_get_intid_list_fn)(void* ctx, dt_prop_ints_t* ints_prop, uint64_t* intids);
typedef void (*intc_dtb_setup_intids_fn)(void* ctx, dt_prop_ints_t* ints_prop);

typedef struct {
    intc_dtb_get_intid_list_fn get_intid_list;
    intc_dtb_setup_intids_fn setup_intids;
} intc_dtb_funcs_t;

void interrupt_init(void);
void interrupt_register_controller(intc_funcs_t* funcs, void* ctx, uint64_t num_handlers);
void interrupt_enable(void);
void interrupt_disable(void);
void interrupt_enable_irq(uint64_t irq);
void interrupt_disable_irq(uint64_t irq);
void interrupt_get_msi(uint64_t* intid_out, uint64_t* data_out, uintptr_t* addr_out);
void interrupt_register_irq_handler(uint64_t irq, irq_handler fn, void* ctx);
bool interrupt_await_reset(uint64_t irq);
bool interrupt_await(uint64_t irq);
void interrupt_get_intid_ctx(void* intid_ctx, void* intids_out, uint64_t* num_intids);

void interrupt_handle_irq_entry(uint64_t vector);
void interrupt_handle_irq(uint64_t irq);

uint64_t interrupt_get_profile_time(uint64_t idx, uint64_t* intid_out);

#endif
