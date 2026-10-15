#pragma once
#include "limine.h"
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define FLAGS_MASK 0xFFF0000000000FFFULL

#define FLAGS_PRESENT  0x1
#define FLAGS_RW       0x2
#define FLAGS_USER     0x4
#define FLAGS_PWT      0x8
#define FLAGS_PCD      0x10
#define FLAGS_ACCESSED 0x20
#define FLAGS_DIRTY    0x40
#define FLAGS_PSE      0x80
#define FLAGS_GLOBAL   0x100
#define FLAGS_COW      0x200
#define FLAGS_PAT      0x1000
#define FLAGS_NX       0x8000000000000000

typedef struct {
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;
} page_address_t;

extern void* hhdm_base;
static inline uint64_t* add_hhdm_to(uint64_t* ptr) {
    return (uint64_t*)((uintptr_t)ptr + (uintptr_t)hhdm_base);
}

static inline uint64_t* page_table_to_address(uint64_t entry) {
    return (uint64_t*)(entry & ~FLAGS_MASK);
}

void init_paging(uintptr_t cr3, struct limine_memmap_response *memmap, uintptr_t hhdm);
void* alloc_region(uintptr_t vaddr, size_t size, uint64_t flags);
void* alloc_mmio_region(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags);
void* alloc_zero_region(uintptr_t vaddr, size_t size, uint64_t flags);
uintptr_t get_available_address();
uintptr_t get_physical_address(uintptr_t virtual_address);
uintptr_t allocate_physical_page();
int validate_user_pointer(void* ptr, uint64_t len, int write);
int validate_user_string(const char *str, uint64_t max_len);
int free_page(void* page);
void free_region(uintptr_t vaddr, size_t size);
void* clone_page_tables(void* pml4_address);
void free_page_tables(void* pml4_address);
int cow_handler(void* faulting_address);
void change_pml4(void* pml4);

page_address_t get_page_entry(uintptr_t addr);
