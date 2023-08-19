
#include <stdint.h>
#include <stdbool.h>

#include "stdlib/bitutils.h"

#include "kernel/console.h"
#include "kernel/drivers.h"
#include "kernel/panic.h"

#include "kernel/lib/libdtb.h"

#include "drivers/aarch64/aarch64.h"

void assert_aarch64_support(void) {

    uint64_t aa64pfr0 = 0;

    READ_SYS_REG(ID_AA64PFR0_EL1, aa64pfr0);

    uint64_t aa64pfr0_el0 = (aa64pfr0 >> AA64PFR0_EL0_SHIFT) & AA64PFR0_EL0_MASK;
    uint64_t aa64pfr0_el1 = (aa64pfr0 >> AA64PFR0_EL1_SHIFT) & AA64PFR0_EL1_MASK;
    uint64_t aa64pfr0_el2 = (aa64pfr0 >> AA64PFR0_EL2_SHIFT) & AA64PFR0_EL2_MASK;
    uint64_t aa64pfr0_el3 = (aa64pfr0 >> AA64PFR0_EL3_SHIFT) & AA64PFR0_EL3_MASK;
    uint64_t aa64pfr0_fp = (aa64pfr0 >> AA64PFR0_FP_SHIFT) & AA64PFR0_FP_MASK;
    uint64_t aa64pfr0_advsimd = (aa64pfr0 >> AA64PFR0_ADVSIMD_SHIFT) & AA64PFR0_ADVSIMD_MASK;
    uint64_t aa64pfr0_gic = (aa64pfr0 >> AA64PFR0_GIC_SHIFT) & AA64PFR0_GIC_MASK;

    bool supported = true;

    console_write("AARCH64 Features:\n");

    switch (aa64pfr0_el0) {
        case AA64PFR0_EL0_AA64_ONLY:
            console_write(" EL0 AA64 ONLY\n");
            break;
        case AA64PFR0_EL0_AA32:
            console_write(" EL0 AA32 SUPPORT\n");
            break;
        default:
            console_write(" EL0 Unknown. Not Supported\n");
            supported = false;
            break;
    }

    switch (aa64pfr0_el1) {
        case AA64PFR0_EL1_AA64_ONLY:
            console_write(" EL1 AA64 ONLY\n");
            break;
        case AA64PFR0_EL1_AA32:
            console_write(" EL1 AA32 SUPPORT\n");
            break;
        default:
            console_write(" EL1 Unknown. Not Supported\n");
            supported = false;
            break;
    }

    switch (aa64pfr0_el2) {
        case AA64PFR0_EL2_AA64_ONLY:
            console_write(" EL2 AA64 ONLY. Not Supported\n");
            break;
        case AA64PFR0_EL2_AA32:
            console_write(" EL2 AA32 ONLY. Not Supported\n");
            break;
        case AA64PFR0_EL2_NOT_IMPLEMENTED:
            console_write(" EL2 NOT IMPLEMENTED\n");
            break;
        default:
            console_write(" EL2 Unknown. Not Supported\n");
            supported = false;
            break;
    }

    switch (aa64pfr0_el3) {
        case AA64PFR0_EL3_AA64_ONLY:
            console_write(" EL3 AA64 ONLY. Not Supported\n");
            break;
        case AA64PFR0_EL3_AA32:
            console_write(" EL3 AA32 ONLY. Not Supported\n");
            break;
        case AA64PFR0_EL3_NOT_IMPLEMENTED:
            console_write(" EL3 NOT IMPLEMENTED\n");
            break;
        default:
            console_write(" EL3 Unknown. Not Supported\n");
            supported = false;
            break;
    }

    switch (aa64pfr0_fp) {
        case AA64PFR0_FP_IMPLEMENTED:
            console_write(" FP IMPLEMENTED\n");
            break;
        case AA64PFR0_FP_NOT_IMPLEMENTED:
            console_write(" FP NOT IMPLEMENTED\n");
            break;
        default:
            console_write(" FP Unknown. Not Supported\n");
            supported = false;
            break;
    }

    switch (aa64pfr0_advsimd) {
        case AA64PFR0_ADVSIMD_IMPLEMENTED:
            console_write(" ADVSIMD IMPLEMENTED\n");
            break;
        case AA64PFR0_ADVSIMD_NOT_IMPLEMENTED:
            console_write(" ADVSIMD NOT IMPLEMENTED\n");
            break;
        default:
            console_write(" ADVSIMD Unknown. Not Supported\n");
            supported = false;
            break;
    }

    switch (aa64pfr0_gic) {
        case AA64PFR0_GIC_SYS_INTERFACE:
            console_write(" GIC System Interface Supported\n");
            break;
        case AA64PFR0_GIC_NO_SYS_INTERFACE:
            console_write(" GIC System Interface Not Supported. Not Supported\n");
            supported = false;
            break;
        default:
            console_write(" GIC System Interface Unknown. Not Supported\n");
            supported = false;
            break;
    }

    /*
    if (!supported) {
        PANIC("Unsupported Target");
    }
    */
   (void)supported;

}

void cortex_a57_discovered(void* ctx) {

    dt_node_t* dt_node = ((discovery_dtb_ctx_t*)ctx)->dt_node; 

    char* name = dt_node->name;

    console_log(LOG_INFO, "Found Cortex-A57: %s:%d\n", name, dt_node->address);
}

static discovery_register_t s_a57_register = {
    .type = DRIVER_DISCOVERY_DTB,
    .dtb = {
        .compat = "arm,cortex-a57"
    },
    .ctxfunc = cortex_a57_discovered
};

void cortex_a57_register(void) {
    register_driver(&s_a57_register);
}

REGISTER_DRIVER(cortex_a57);