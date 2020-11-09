
#ifndef __ELF_TYPES_H__
#define __ELF_TYPES_H__

#include <stdint.h>

#define EI_NIDENT 16
#define ELF_HEADER_LEN 64

typedef struct __attribute__((__packed__)) {
    char magic[4];      // Magic String '\x7FELF'
    uint8_t class;      // Class - 32 or 64 bit
    uint8_t data;       // Data Encoding of the file
    uint8_t version;    // Version of the ELF standard. Currently 1
    uint8_t osabi;      // Operating system and ABI of the file
    uint8_t abiversion; // ABI Version
    uint8_t pad[7];
} _elf64_ident_t;

typedef struct __attribute__((__packed__)) {
    _elf64_ident_t e_ident;  // Identification
    uint16_t e_type;         // Type of object
    uint16_t e_machine;      // Architecture
    uint32_t e_version;      // ELF Version
    uint64_t e_entry;        // Virtual Address of entry address
    uint64_t e_phoff;        // Program Header file offset
    uint64_t e_shoff;        // Section Header file offset
    uint32_t e_flags;        // Machine specific flags
    uint16_t e_ehsize;       // ELF header size
    uint16_t e_phentsize;    // Program Header Entry Size
    uint16_t e_phnum;        // Program Header Number of Entries
    uint16_t e_shentsize;    // Section Header Entry size
    uint16_t e_shnum;        // Section Header Number of Entries
    uint16_t e_shstrndx;     // String table index into Section Header
} _elf64_ehdr_t;

typedef enum {
    ELFCLASS32 = 1,
    ELFCLASS64 = 2
} _elf_class_t;

typedef enum {
    ELFDATA2LSB = 1,
    ELFDATA2MSB = 2
} _elf_data_t;

typedef enum {
    ELFOSABI_NONE = 0,
} _elf_osabi_t;

typedef enum {
    ET_NONE = 0,
    ET_REL = 1,
    ET_EXEC = 2,
    ET_DYN = 3,
    ET_CORE = 4,
    ET_LOOS = 0xFE00,
    ET_HIOS = 0xFEFF,
    ET_LOPROC = 0xFF00,
    ET_HIPROFC = 0xFFFF
} _elf_etype_t;

typedef enum {
    EM_NONE = 0,
    EM_AARCH64 = 183
} _elf_emachine_t;

typedef enum {
    EV_NONE = 0,
    EV_CURRENT = 1
} _elf_eversion_t;


typedef struct __attribute__((__packed__)) {
    uint32_t sh_name;      // Name offset in string table
    uint32_t sh_type;      // Type
    uint64_t sh_flags;     // Flags
    uint64_t sh_addr;      // Virtual Address of section
    uint64_t sh_offset;    // File Offset
    uint64_t sh_size;      // Size in bytes
    uint32_t sh_link;      // Index of associated section
    uint32_t sh_info;      // Info
    uint64_t sh_addralign; // Required alignment of the section
    uint64_t sh_entsize;   // Size of each entry in bytes
} _elf64_shdr_t;

typedef enum {
    SHT_NULL = 0,     // Unused section header
    SHT_PROGBITS = 1, // Information defined by the program
    SHT_SYMTAB = 2,   // Linker Symbol Table
    SHT_STRTAB = 3,   // Linker String Table
    SHT_RELA = 4      // RELA type relocation entries
} _elf_shtype_t;

typedef enum {
    SHF_WRITE = 1,    // Writable data
    SHF_ALLOC = 2,    // Allocate section in program image
    SHF_EXECINSTR = 4 // Executable instructions
} _elf_shflags_t;

typedef struct __attribute__((__packed__)) {
    uint32_t st_name;   // Name offset in string table
    uint8_t st_info;    // Symbol type and binding attributes
    uint8_t st_other;   // Reserved
    uint16_t st_shndx;  // Section in which the symbol is defined
    uint64_t st_value;  // Value of the symbol
    uint64_t st_size;   // Size associated with the symbol
} _elf64_sym_t;

typedef enum {
    STB_LOCAL = 0,  // Not visible outside the object file
    STB_GLOBAL = 1, // Visible to all object files
    STB_WEAK = 2,   // Global scobe with weaker precedence than global
} _elf_syminfo_t;

typedef enum {
    STT_NOTYPE = 0,  // No type specified
    STT_OBJECT = 1,  // Data Object
    STT_FUNC = 2,    // Function Entry
    STT_SECTION = 3, // Associated with a section
    STT_FILE = 4,    // Source file associated with the object
} _elf_symtype_t;


typedef struct __attribute__((__packed__)) {
    uint32_t p_type;   // Type
    uint32_t p_flags;  // Attributes
    uint64_t p_offset; // Offset in file
    uint64_t p_vaddr;  // Virtual Address in memory
    uint64_t p_paddr;  // Reserved
    uint64_t p_filesz; // Size of segment in file
    uint64_t p_memsz;  // Size of segment in memory
    uint64_t p_align;  // Alignment
} _elf64_phdr_t;

typedef enum {
    PT_NULL = 0, // Unused
    PT_LOAD = 1  // Loadable segment
} _elf64_ptype_t;

typedef enum {
    PF_X = 1, // Execute permission
    PF_W = 2, // Write permission
    PF_R = 4, // Read permission
    PF_PERM_MASK = 0x7
} _elf64_pflags_t;

#endif