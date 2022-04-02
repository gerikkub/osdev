
#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>
#include <stdbool.h>

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
    _val &= ~BIT(7); /* IRQ Mask */ \
    WRITE_SYS_REG(DAIF, _val); \
    (void)_val; \
    } while(0)

#define DISABLE_IRQ() \
    do { \
    volatile uint64_t _val; \
    READ_SYS_REG(DAIF, _val);  \
    _val |= BIT(7); /* IRQ Mask */ \
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

typedef struct {
    intc_enable_fn enable;
    intc_disable_fn disable;
    intc_enable_irq_fn enable_irq;
    intc_disable_irq_fn disable_irq;
    intc_irq_trigger_fn set_irq_trigger;
    intc_get_msi_fn get_msi;
} intc_funcs_t;

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

void interrupt_handle_irq(uint64_t irq);

#endif
