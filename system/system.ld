
ENTRY(_start)

addr_data = 0x100000;
addr_text = 0x1000;

MEMORY
{
    /* TEXT (rx) : ORIGIN = 0x000040000000, LENGTH = 261120M */ /* 255 GB */
    /* DATA (rw) : ORIGIN = 0x00E000000000, LENGTH = 131072M */ /* 128 GB */
    TEXT (rx) : ORIGIN = 0x000040000000, LENGTH = 64M
    DATA (rw) : ORIGIN = 0x000060000000, LENGTH = 64M
}


SECTIONS
{
    . = addr_text;
    .text :
    {
        . = ALIGN(4096);
        _text_start = .;
        *(.text*)
        *(.rodata*)
        _text_padding = .;
        . = ALIGN(4096);
        _text_end = .;
    } >TEXT

    . = addr_data;
    .data :
    {
        . = ALIGN(4096);
        _data_start = .;
        *(.data*)
        _data_padding = .;
        . = ALIGN(4096);
        _data_end = .;
    } >DATA

    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.bss*)
        _bss_padding = .;
        . = ALIGN(4096);
        _bss_end = .;
    } >DATA

}

