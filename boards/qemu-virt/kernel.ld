
ENTRY(_start)

bootstrap_base = 0x100000;
bootstrap_text_size = 0x4000;
bootstrap_data_size = 0x4000;

kernel_base = 0x0;

highmem_base = 0xFFFF000000000000;

ram_base = 0x40000000;

ram_size = 512M;
flash_size = 0x7000000;

heap_size = 128M;


SECTIONS
{

    . = bootstrap_base;
    .bootstrap.text : {
        _bootstrap_text_start = .;
        *(.bootstrap.text*)
        . = ALIGN(4096);
        _bootstrap_text_end = .;
    }

    . = ram_base;
    .bootstrap.data 0x40000000: {
        _bootstrap_data_start = .;
        *(.bootstrap.data*)
        . = ALIGN(4096);
        _bootstrap_data_end = .;
    }

    .bootstrap.data : {
        *(.bootstrap.data*)
    }

    . = highmem_base + bootstrap_base + bootstrap_text_size;
    .text :  AT (bootstrap_base + bootstrap_text_size)
    {
        . = ALIGN(4096);
        _text_start = .;
        *(.text*)
        *(.rodata*)
        . = ALIGN(2048);
         _exception_vector_base = .;
        *(.exception_vector)
        . = ALIGN(4096);
        _text_end = .;
        . = ALIGN(4096);
    }

    . = highmem_base + ram_base + bootstrap_data_size;

    .data : AT(ram_base + bootstrap_data_size)
    {
        . = ALIGN(4096);
        _data_start = .;
        *(.data*)
        . = ALIGN(8);
        _driver_init_start = .;
        KEEP(*(.driver_init))
        _driver_init_end = .;
        . = ALIGN(4096);
        _data_end = .;
    }

    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.bss*)
        . = ALIGN(4096);
        _stack_limit = .;
        . += 4096;
        _stack_base = .;
        . = ALIGN(4096);
        _exception_stack_limit = .;
        . += 4096;
        _exception_stack_base = .;
        . = ALIGN(4096);
        _heap_base = .;
        . += heap_size;
        _heap_limit = .;
        _bss_end = .;
    }

}
