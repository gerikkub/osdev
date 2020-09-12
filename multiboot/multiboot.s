
.section .multiboot

# Magic
# .word 0x1BADB002

# # Flags
# .word 0x00000000

# # Checksum
# .word 0xE4524FFE

# # header_addr
# .word 0

# # load_addr
# .word 0

# # load_end_addr
# .word 0

# # bss_end_addr
# .word 0

# # entry_addr
# .word 0

# # mode_type
# .word 0

# # width
# .word 0

# # height
# .word 0

# # depth
# .word 0

magicboot:

# Magic
.word 0xE85250D6

# Architecture
.word 4

# Header Length
.word 68

# Checksum
.word 0x17ADAEE2

# Entry address tag
.hword 3
.hword 0
.word 12
.word _start

# End Tag
.hword 0
.hword 0
.word 8
