
.arch armv8-a

.text

.global vmem_initialize_asm
.global vmem_set_l0_table_asm

vmem_initialize_asm:
    msr TCR_EL1, x0

    ret

vmem_set_l0_table_asm:
    msr TTBR1_EL1, x0

    ret


