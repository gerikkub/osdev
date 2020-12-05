
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/elf_types.h"
#include "kernel/elf.h"
#include "kernel/task.h"
#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/assert.h"
#include "kernel/kmalloc.h"


static elf_result_t elf_valid(uint8_t* elf_data, uint64_t elf_size) {

    if (elf_size < 64) {
        return ELF_BADHEADER;
    }

    _elf64_ehdr_t* header = (_elf64_ehdr_t*)elf_data;

    // Validate Magic
    if (memcmp(header->e_ident.magic, "\x7F""ELF", 4) != 0) {
        return ELF_BADMAGIC;
    }

    // Validate ident fields
    if (!(header->e_ident.class == ELFCLASS64 &&
          header->e_ident.data == ELFDATA2LSB &&
          header->e_ident.version == EV_CURRENT &&
          header->e_ident.osabi == 0 &&
          header->e_ident.abiversion == 0)) {
        return ELF_BADIDENT;
    }

    // Valider header fields
    if (!(header->e_type == ET_EXEC &&
          header->e_machine == EM_AARCH64 &&
          header->e_version == EV_CURRENT &&
          header->e_entry != 0 &&
          header->e_phnum > 0 &&
          header->e_phentsize == sizeof(_elf64_phdr_t) &&
          header->e_shentsize == sizeof(_elf64_shdr_t) &&
          header->e_phoff < elf_size &&
          header->e_shoff < elf_size)) {
        return ELF_BADHEADER;
    }

    // Validate program header 
    if (header->e_phoff + (header->e_phentsize * header->e_phnum) > elf_size) {
        return ELF_BADHEADER;
    }

    // Validate section header
    if (header->e_shoff + (header->e_shentsize * header->e_shnum) > elf_size) {
        return ELF_BADHEADER;
    }
 
    return ELF_VALID;
}

static elf_result_t elf_add_memspace_entry(memory_space_t* memspace, _elf64_phdr_t* phdr, uint8_t* elf_data, uint64_t elf_size) {
    /* Parse a segment andd add a memoryspace entry as appropriate */

    ASSERT(memspace != NULL);
    ASSERT(phdr != NULL);

    // Return early if the PHDR entry is empty
    if (phdr->p_memsz == 0 && phdr->p_filesz == 0) {
        return ELF_EMPTYPHDR;
    }

    // Validate permissions of the segment. No Write Execute allowed
    if (!((phdr->p_flags & PF_PERM_MASK) == (PF_R) ||         // Read Only
          (phdr->p_flags & PF_PERM_MASK) == (PF_R | PF_W) ||  // Read Write
          (phdr->p_flags & PF_PERM_MASK) == (PF_R | PF_X))) { // Read Execute
        return ELF_BADPHDR;
    }

    // Validate that the segment fits inside the file
    if (phdr->p_offset + phdr->p_filesz >= elf_size) {
        return ELF_BADPHDR;
    }

    // Validate that the file memory, if present, is valid
    if ((phdr->p_offset > 0 && phdr->p_filesz == 0) ||
        (phdr->p_filesz == 0 && phdr->p_offset > 0)) {
        return ELF_BADPHDR;
    }

    // Validate that the memory size of this entries is at
    // least as big as the filesize
    if (phdr->p_filesz > phdr->p_memsz) {
        return ELF_BADPHDR;
    }

    // Allocate physical memory for the segment
    uint8_t* phy_mem = kmalloc_phy(phdr->p_memsz);
    if (phy_mem == NULL) {
        return ELF_CANTALLOC;
    }

    // Copy file data into physical memory
    if (phdr->p_offset > 0) {
        memcpy(phy_mem, &elf_data[phdr->p_offset], phdr->p_filesz);
    }

    // Translate memory permission flags
    uint32_t memspace_flags = 0;
    if ((phdr->p_flags & PF_PERM_MASK) == PF_R) {
        memspace_flags = MEMSPACE_FLAG_PERM_URO;
    } else if ((phdr->p_flags & PF_PERM_MASK) == (PF_R | PF_W)) {
        memspace_flags = MEMSPACE_FLAG_PERM_URW;
    } else if ((phdr->p_flags & PF_PERM_MASK) == (PF_R | PF_X)) {
        memspace_flags = MEMSPACE_FLAG_PERM_URE;
    } else {
        ASSERT(0);
    }

    // Create the memory space entry
    memory_entry_phy_t phdr_entry = {
        .start = phdr->p_vaddr,
        .end = PAGE_CEIL((phdr->p_vaddr + phdr->p_memsz)),
        .type = MEMSPACE_PHY,
        .flags = memspace_flags,
        .phy_addr = KSPACE_TO_PHY(phy_mem),
        .kmem_addr = (uint64_t)phy_mem
    };

    // Add the entry to the memoryspace
    bool result = memspace_add_entry_to_memory(memspace, (memory_entry_t*)&phdr_entry);
    if (!result) {
        return ELF_CANTALLOC;
    }

    return ELF_VALID;
}

uint64_t create_elf_task(uint8_t* elf_data, uint64_t elf_size, elf_result_t* result, bool system_task) {

    ASSERT(elf_data != NULL);
    ASSERT(elf_size > 0);

    elf_result_t tmp_result;

    // Validate this is a valid ELF that is loadable
    tmp_result = elf_valid(elf_data, elf_size);
    if (tmp_result != ELF_VALID) {
        if (result != NULL) {
            *result = tmp_result;
        }
        return 0;
    }

    // Start building a memoryspace for this object
    memory_space_t elf_space;
    elf_space.entries = kmalloc_phy(ELF_MAX_MEMSPACE_ENTRIES * sizeof(memory_entry_t));
    elf_space.num = 0;
    elf_space.maxnum = ELF_MAX_MEMSPACE_ENTRIES;
    elf_space.valloc_ctx.systemspace_end = USER_ADDRSPACE_SYSTEM;

    if (elf_space.entries == NULL) {
        if (result != NULL) {
            *result = ELF_CANTALLOC;
        }
        return 0;
    }

    _elf64_ehdr_t* header = (_elf64_ehdr_t*)elf_data;
    _elf64_phdr_t* phdr = (_elf64_phdr_t*)(elf_data + header->e_phoff);

    // Loop through the segments and add a memory entry for
    for (uint64_t idx = 0; idx < header->e_phnum; idx++) {
        switch (phdr[idx].p_type) {
            case PT_LOAD:
                tmp_result = elf_add_memspace_entry(&elf_space, &phdr[idx], elf_data, elf_size);
                if (tmp_result == ELF_EMPTYPHDR) {
                    continue;
                }
                break;
            case PT_NULL:
                tmp_result = ELF_VALID;
                break;
            default:
                tmp_result = ELF_UNHANDLED_PHDR;
                break;
        }

        if (tmp_result != ELF_VALID) {
            memspace_deallocate(&elf_space);
            if (result != NULL) {
                *result = tmp_result;
            }
            return 0;
        }
    }

    // Allocate a stack for the task
    uint8_t* stack_phy_space = kmalloc_phy(USER_STACK_SIZE);
    if (stack_phy_space == NULL) {
        memspace_deallocate(&elf_space);
        if (result != NULL) {
            *result = ELF_CANTALLOC;
        }
        return 0;
    }

    // Add a stack entry to the memoryspace
    uintptr_t stack_base = USER_ADDRSPACE_STACKTOP - USER_STACK_GUARD_PAGES;
    uintptr_t stack_limit = stack_base - USER_STACK_SIZE;
    memory_entry_stack_t elf_stack = {
        .start = USER_ADDRSPACE_STACKTOP - USER_STACK_SIZE - 2*USER_STACK_GUARD_PAGES,
        .end = USER_ADDRSPACE_STACKTOP,
        .type = MEMSPACE_STACK,
        .flags = MEMSPACE_FLAG_PERM_URW,
        .phy_addr = KSPACE_TO_PHY((uintptr_t)stack_phy_space),
        .base = stack_base,
        .limit = stack_limit,
        .maxlimit = stack_limit
    };
    bool memspace_result;
    memspace_result = memspace_add_entry_to_memory(&elf_space, (memory_entry_t*)&elf_stack);
    if (!memspace_result) {
        memspace_deallocate(&elf_space);
        kfree_phy(stack_phy_space);
        if (result != NULL) {
            *result = ELF_CANTALLOC;
        }
        return 0;
    }

    uint64_t tid;
    if (system_task) {
        tid = create_system_task(KERNEL_STD_STACK_SIZE,
                                 stack_base,
                                 USER_STACK_SIZE,
                                 &elf_space,
                                 (task_f)header->e_entry,
                                 NULL);
    } else {
        ASSERT(0);
        tid = 0;
        /*tid = create_user_task(TASK_STD_STACK_SIZE,
                               KERNEL_STD_STACK_SIZE,
                               memspace,
                               header->e_entry,
                               NULL);
                               */
    }

    if (tid == 0) {
        memspace_deallocate(&elf_space);
        if (result != NULL) {
            *result = ELF_BADTASK;
        }
        return 0;
    }

    if (result != NULL) {
        *result = ELF_VALID;
    }
    return tid;
}


