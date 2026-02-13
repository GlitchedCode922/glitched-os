#include "elf.h"
#include "../mount.h"
#include "../memory/mman.h"
#include "../memory/paging.h"
#include <stdint.h>

int check_nx_support() {
    uint32_t eax, ebx, ecx, edx;

    eax = 0x80000001;
    __asm__ volatile(
        "cpuid"
        : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        :
    );

    // Bit 20 of edx indicates NX support
    return (edx & (1 << 20)) != 0;
}

int is_elf(const char* path) {
    Elf64_Ehdr ehdr;
    read_file(path, (uint8_t*)&ehdr, 0, sizeof(ehdr));

    // Check ELF magic number
    if (ehdr.e_ident[0] != ELFMAG0 || ehdr.e_ident[1] != ELFMAG1 ||
        ehdr.e_ident[2] != ELFMAG2 || ehdr.e_ident[3] != ELFMAG3) {
        return 0; // Not an ELF file
    }

    return 1; // Valid ELF file
}

int is_compatible_binary(const char* path) {
    if (!is_elf(path)) return 0; // Not an ELF file

    Elf64_Ehdr ehdr;
    read_file(path, (uint8_t*)&ehdr, 0, sizeof(ehdr));

    // Check ELF class
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        return 0; // Not a 64-bit ELF file
    }

    // Check ELF data encoding
    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        return 0; // Not little-endian
    }

    // Check ELF type
    if (ehdr.e_type != ET_EXEC) {
        return 0; // Not an executable
    }

    return 1; // Compatible binary
}

void* load_elf(const char* path, void** brk) {
    if (!is_compatible_binary(path)) {
        return 0; // Not a compatible ELF binary
    }

    Elf64_Ehdr ehdr;

    // Read ELF header
    read_file(path, (uint8_t*)&ehdr, 0, sizeof(Elf64_Ehdr));

    Elf64_Phdr phdrs[ehdr.e_phnum];

    // Read program headers
    read_file(path, (uint8_t*)&phdrs, ehdr.e_phoff, ehdr.e_phnum * sizeof(Elf64_Phdr));

    void* break_addr = NULL;
    // Load segments into memory
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            // Allocate memory for the segment
            uint64_t flags = FLAGS_PRESENT | FLAGS_USER;
            flags |= FLAGS_RW; // Proper write permission management requires mprotect(), which doesn't exist yet
            if (!(phdrs[i].p_flags & PF_X) && check_nx_support()) {
                flags |= FLAGS_NX; // Not executable
            }
            void* segment_start = alloc_region(phdrs[i].p_vaddr, phdrs[i].p_memsz, flags);
            if (!segment_start) {
                return 0; // Failed to allocate memory for segment
            }

            // Read the segment data from the file
            read_file(path, (uint8_t*)segment_start, phdrs[i].p_offset, phdrs[i].p_filesz);

            // Update break address
            uint64_t segment_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            if (!break_addr || segment_end > (uint64_t)break_addr) {
                break_addr = (void*)segment_end;
            }
        }
    }

    if (brk) *brk = (void*)(((uintptr_t)break_addr / PAGE_SIZE + 1) * PAGE_SIZE); // Update the break address
    return (void*)ehdr.e_entry; // Successfully loaded ELF binary
}
