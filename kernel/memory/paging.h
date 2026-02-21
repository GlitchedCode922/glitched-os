#pragma once
#include "../limine.h"
#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

#define FLAGS_PRESENT  0x1
#define FLAGS_RW       0x2
#define FLAGS_USER     0x4
#define FLAGS_PWT      0x8
#define FLAGS_PCD      0x10
#define FLAGS_ACCESSED 0x20
#define FLAGS_DIRTY    0x40
#define FLAGS_PSE      0x80
#define FLAGS_GLOBAL   0x100
#define FLAGS_PAT      0x1000
#define FLAGS_NX       0x8000000000000000

typedef struct {
    uint16_t pml4_index;
    uint16_t pdpt_index;
    uint16_t pd_index;
    uint16_t pt_index;
} page_address_t;

void init_paging(uintptr_t cr3, struct limine_memmap_response *memmap, uintptr_t hhdm);
void* alloc_page(uintptr_t addr, uint64_t flags);
void* alloc_mmio_page(uintptr_t vaddr, uintptr_t paddr, uint64_t flags);
uintptr_t get_physical_address(uintptr_t virtual_address);
int free_page(void* page);
int check_reserved(uintptr_t addr);
int check_used(uintptr_t addr);
void* clone_page_tables(void* pml4_address);
void change_pml4(void* pml4);

page_address_t get_page_entry(uintptr_t addr);
