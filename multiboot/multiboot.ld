
ENTRY(_start)

PHDRS
{
    text PT_LOAD ;
    data PT_LOAD ;
}

MEMORY
{
/* Origin needs to be > 0x100000 for qemu to load */
FLASH (rx)      : ORIGIN = 0x00100000, LENGTH = 0x7000000 
RAM (xrw)      : ORIGIN = 0x40000000, LENGTH = 256M
}

SECTIONS
{

    .text : {
        . = ALIGN(4096);
        _text_start = .;
        *(.text*)
        *(.rodata*)
        . = ALIGN(2048);
         _exception_vector_base = .;
        *(.exception_vector)
        . = ALIGN(4096);
        _text_end = .;
    } >FLASH :text


    .data :
    {
        . = ALIGN(4096);
        _data_start = .;
        *(.data*)
        . = ALIGN(4096);
        _data_end = .;
    } >RAM :data

    .bss (NOLOAD) :
    {
        . = ALIGN(4096);
        _bss_start = .;
        *(.bss*)
        . = ALIGN(4096);
        _stack_limit = .;
        . += 4096;
        _stack_base = .;
        _bss_end = .;
    } >RAM :data

}
