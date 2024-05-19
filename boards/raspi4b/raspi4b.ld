
ENTRY(_start)

bootstrap_base = 0x80000;
bootstrap_data_base = 0x84000;
bootstrap_text_size = 0x4000;
bootstrap_data_size = 0x4000;

kernel_base = 0x0;

highmem_base = 0xFFFF000000000000;

ram_base = 0x100000;

ram_size = 128M;
flash_size = 0x7000000;

heap_size = 64M;


SECTIONS
{

    . = bootstrap_base;
    .bootstrap.text : {
        *(.bootstrap.text.first*)
        *(.bootstrap.text*)
    }

    . = bootstrap_data_base;
    .bootstrap.data : {
        *(.bootstrap.data.first*)
        *(.bootstrap.data*)
    }

    . = highmem_base + bootstrap_base + bootstrap_text_size + bootstrap_data_size;
    .text :  AT (bootstrap_base + bootstrap_text_size + bootstrap_data_size)
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
