#pragma once

#include <stdint.h>

// --- ELF Magic Number ---
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

static const unsigned char ELFMAG[4] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};

// --- ELF Identifiers (e_ident) ---
#define EI_CLASS 4          // ELF class (32 or 64 bits)
#define EI_DATA  5          // Data encoding (little-endian or big-endian)
#define EI_VERSION 6       // ELF version
#define EI_OSABI 7         // OS/ABI identification
#define EI_ABIVERSION 8    // ABI version
#define EI_PAD 9           // Start of padding bytes

// --- ELF Class Types (e_ident[EI_CLASS]) ---
#define ELFCLASSNONE 0     // Invalid class
#define ELFCLASS32   1     // 32-bit objects
#define ELFCLASS64   2     // 64-bit objects

// --- Data Encoding Types (e_ident[EI_DATA]) ---
#define ELFDATANONE 0      // Invalid data encoding
#define ELFDATA2LSB 1      // Little-endian
#define ELFDATA2MSB 2      // Big-endian

// --- ELF File Types (e_type) ---
#define ET_NONE 0        // No file type
#define ET_REL  1        // Relocatable file
#define ET_EXEC 2        // Executable file
#define ET_DYN  3        // Shared object file
#define ET_CORE 4        // Core file

// --- Architecture Types (e_machine) ---
#define EM_NONE   0        // No machine
#define EM_386    3        // Intel 80386
#define EM_X86_64 62       // AMD x86-64 architecture
#define EM_ARM    40       // ARM
#define EM_AARCH64 183     // ARM 64-bit architecture

// --- Program Header Types (p_type) ---
#define PT_NULL    0       // Unused segment
#define PT_LOAD    1       // Loadable segment
#define PT_DYNAMIC 2       // Dynamic linking info
#define PT_INTERP  3       // Interpreter path
#define PT_NOTE    4       // Auxiliary info
#define PT_SHLIB   5       // Reserved
#define PT_PHDR    6       // Program header table itself
#define PT_TLS     7       // Thread-local storage segment

// --- Program Header Flags (p_flags) ---
#define PF_X 0x1           // Execute
#define PF_W 0x2           // Write
#define PF_R 0x4           // Read

// --- ELF64 Header ---
typedef struct {
    unsigned char e_ident[16];   // Magic number and other info
    uint16_t      e_type;        // Object file type
    uint16_t      e_machine;     // Architecture
    uint32_t      e_version;     // Object file version
    uint64_t      e_entry;       // Entry point virtual address
    uint64_t      e_phoff;       // Program header table file offset
    uint64_t      e_shoff;       // Section header table file offset
    uint32_t      e_flags;       // Processor-specific flags
    uint16_t      e_ehsize;      // ELF header size in bytes
    uint16_t      e_phentsize;   // Program header table entry size
    uint16_t      e_phnum;       // Program header table entry count
    uint16_t      e_shentsize;   // Section header table entry size
    uint16_t      e_shnum;       // Section header table entry count
    uint16_t      e_shstrndx;    // Section header string table index
} Elf64_Ehdr;

// --- ELF64 Program Header ---
typedef struct {
    uint32_t p_type;    // Segment type
    uint32_t p_flags;   // Segment flags
    uint64_t p_offset;  // Segment file offset
    uint64_t p_vaddr;   // Segment virtual address
    uint64_t p_paddr;   // Segment physical address (unused)
    uint64_t p_filesz;  // Segment size in file
    uint64_t p_memsz;   // Segment size in memory
    uint64_t p_align;   // Segment alignment
} Elf64_Phdr;

int is_elf(const char *path);
int is_compatible_binary(const char *path);
void* load_elf(const char *path);
