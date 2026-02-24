#include "paging.h"
#include "mman.h"
#include "../panic.h"
#include <stddef.h>
#include <stdint.h>

void* pml4_address = NULL;
void* hhdm_base = 0;
struct limine_memmap_response *memory_map = NULL;
char* memory_bitmap = NULL;
uint64_t page_count = 0;

#define HIGHER_LEVEL_FLAGS (FLAGS_PRESENT | FLAGS_RW | FLAGS_USER)

int check_used(uintptr_t addr) {
    if (!memory_bitmap) return 1;
    size_t page = addr / PAGE_SIZE;

    if (page >= page_count)
        return 0; // invalid address

    size_t byte_index = page / 8;
    size_t bit_index  = page % 8;

    return (memory_bitmap[byte_index] & (1 << bit_index)) != 0;
}

int check_reserved(uintptr_t addr) {
    // Check if the address is reserved in the memory map
    if (memory_map == NULL) {
        return -1;
    }

    for (size_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *entry = memory_map->entries[i];
        if (entry->type == LIMINE_MEMMAP_RESERVED && 
            addr >= entry->base && 
            addr < entry->base + entry->length) {
            return 1; // Address is reserved
        }
    }
    return 0; // Address is not reserved
}

uint64_t* add_hhdm_to(uint64_t* ptr) {
    return (uint64_t*)((uintptr_t)ptr + (uintptr_t)hhdm_base);
}

uint64_t* page_table_to_address(uint64_t entry) {
    return (uint64_t*)(entry & PAGE_MASK & ~FLAGS_NX);
}

uintptr_t get_physical_address(uintptr_t virtual_address) {
    page_address_t entry = get_page_entry(virtual_address);

    uint64_t* pml4 = (uint64_t*)pml4_address;
    if (!(pml4[entry.pml4_index] & FLAGS_PRESENT)) return 0;

    uint64_t* pdpt = add_hhdm_to(page_table_to_address(pml4[entry.pml4_index]));
    if (!(pdpt[entry.pdpt_index] & FLAGS_PRESENT)) return 0;

    uint64_t* pd = add_hhdm_to(page_table_to_address(pdpt[entry.pdpt_index]));
    if (!(pd[entry.pd_index] & FLAGS_PRESENT)) return 0;

    uint64_t* pt = add_hhdm_to(page_table_to_address(pd[entry.pd_index]));
    if (!(pt[entry.pt_index] & FLAGS_PRESENT)) return 0;

    return (uintptr_t)page_table_to_address(pt[entry.pt_index]);
}

void init_paging(uintptr_t cr3, struct limine_memmap_response *memmap, uintptr_t hhdm) {
    if (cr3 == 0 || memmap == NULL) panic("init_paging failed");

    pml4_address = (void*)hhdm + cr3;
    hhdm_base = (void*)hhdm;
    memory_map = memmap;

    // --- Find highest physical address ---
    uintptr_t highest = 0;

    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        uintptr_t end = e->base + e->length;
        if (end > highest) highest = end;
    }

    page_count = highest / PAGE_SIZE;
    size_t bitmap_size = (page_count + 7) / 8;
    uintptr_t bitmap_phys = 0;

    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];

        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        if (e->length >= bitmap_size) {
            bitmap_phys = e->base;
            break;
        }
    }

    if (!bitmap_phys) panic("Cannot allocate paging bitmap");
    memory_bitmap = (char*)add_hhdm_to((uint64_t*)bitmap_phys);

    // Clear the bitmap and mark its pages as used
    for (size_t i = 0; i < bitmap_size; i++) memory_bitmap[i] = 0;
    for (uintptr_t addr = bitmap_phys; addr < bitmap_phys + bitmap_size; addr += PAGE_SIZE) {
        size_t page = addr / PAGE_SIZE;
        memory_bitmap[page / 8] |= (1 << (page % 8));
    }
}

page_address_t get_page_entry(uintptr_t addr) {
    // Calculate the indices for the page table entries
    page_address_t entry;
    entry.pml4_index = (addr >> 39) & 0x1FF; // Bits 39-47
    entry.pdpt_index = (addr >> 30) & 0x1FF; // Bits 30-39
    entry.pd_index = (addr >> 21) & 0x1FF;   // Bits 21-29
    entry.pt_index = (addr >> 12) & 0x1FF;   // Bits 12-20

    return entry;
}

uintptr_t get_available_address() {
    for (int i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *entry = memory_map->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uintptr_t start = entry->base;
            uintptr_t end = entry->base + entry->length;
            for (uintptr_t i = start; i < end; i += PAGE_SIZE) {
                if (i >= end) {
                    break; // No more space in this region
                }
                // Check if the address is not reserved or used
                if (check_reserved(i) == 0 && check_used(i) == 0) {
                    return i; // Return the first available address
                }
            }
        }
    }
    panic("Out of memory: No available address found in the memory map");
}

void* allocate_page_table() {
    uintptr_t addr = get_available_address();
    size_t page = addr / PAGE_SIZE;
    memory_bitmap[page / 8] |= (1 << (page % 8));

    memset(add_hhdm_to((uint64_t*)addr), 0, PAGE_SIZE);
    return (void*)addr;
}

void* alloc_page(uintptr_t vaddr, uint64_t flags) {
    page_address_t idx = get_page_entry(vaddr);
    uint64_t *pml4 = (uint64_t*)pml4_address;

    // --- PML4 level ---
    uint64_t pml4e = pml4[idx.pml4_index];
    if (!(pml4e & FLAGS_PRESENT)) {
        uint64_t *new_pdpt = allocate_page_table();
        uint64_t *new_pdpt_hhdm = add_hhdm_to(new_pdpt);
        if (!new_pdpt) panic("alloc_page: cannot allocate PDPT");
        // clear entries
        for (int i = 0; i < 512; i++) new_pdpt_hhdm[i] = 0;
        // install
        pml4[idx.pml4_index] = ((uintptr_t)new_pdpt & PAGE_MASK)
                              | (flags & HIGHER_LEVEL_FLAGS)
                              | FLAGS_PRESENT;
        pml4e = pml4[idx.pml4_index];
    }
    uint64_t *pdpt = add_hhdm_to(page_table_to_address(pml4e));

    // --- PDPT level ---
    uint64_t pdpte = pdpt[idx.pdpt_index];
    if (!(pdpte & FLAGS_PRESENT)) {
        uint64_t *new_pd = allocate_page_table();
        uint64_t *new_pd_hhdm = add_hhdm_to(new_pd);
        if (!new_pd) panic("alloc_page: cannot allocate PD");
        for (int i = 0; i < 512; i++) new_pd_hhdm[i] = 0;
        pdpt[idx.pdpt_index] = ((uintptr_t)new_pd & PAGE_MASK)
                              | (flags & HIGHER_LEVEL_FLAGS)
                              | FLAGS_PRESENT;
        pdpte = pdpt[idx.pdpt_index];
    }
    uint64_t *pd = add_hhdm_to(page_table_to_address(pdpte));

    // --- PD level ---
    uint64_t pde = pd[idx.pd_index];
    if (!(pde & FLAGS_PRESENT)) {
        uint64_t *new_pt = allocate_page_table();
        uint64_t *new_pt_hhdm = add_hhdm_to(new_pt);
        if (!new_pt) panic("alloc_page: cannot allocate PT");
        for (int i = 0; i < 512; i++) new_pt_hhdm[i] = 0;
        pd[idx.pd_index] = ((uintptr_t)new_pt & PAGE_MASK)
                          | (flags & HIGHER_LEVEL_FLAGS)
                          | FLAGS_PRESENT;
        pde = pd[idx.pd_index];
    }
    uint64_t *pt = add_hhdm_to(page_table_to_address(pde));

    // --- PT level ---
    if (pt[idx.pt_index] & FLAGS_PRESENT) {
        panic("alloc_page: virtual 0x%p already mapped", (void*)vaddr);
    }

    // allocate a physical page and map it
    uintptr_t phys = get_available_address();
    pt[idx.pt_index] = (phys & PAGE_MASK)
                      | flags
                      | FLAGS_PRESENT;
    size_t page = phys / PAGE_SIZE;
    memory_bitmap[page / 8] |= (1 << (page % 8));

    return (void*)vaddr;
}

void* alloc_mmio_page(uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    page_address_t idx = get_page_entry(vaddr);
    uint64_t *pml4 = (uint64_t*)pml4_address;

    // --- PML4 level ---
    uint64_t pml4e = pml4[idx.pml4_index];
    if (!(pml4e & FLAGS_PRESENT)) {
        uint64_t *new_pdpt = allocate_page_table();
        uint64_t *new_pdpt_hhdm = add_hhdm_to(new_pdpt);
        if (!new_pdpt) panic("alloc_page: cannot allocate PDPT");
        // clear entries
        for (int i = 0; i < 512; i++) new_pdpt_hhdm[i] = 0;
        // install
        pml4[idx.pml4_index] = ((uintptr_t)new_pdpt & PAGE_MASK)
                              | (flags & HIGHER_LEVEL_FLAGS)
                              | FLAGS_PRESENT;
        pml4e = pml4[idx.pml4_index];
    }
    uint64_t *pdpt = add_hhdm_to(page_table_to_address(pml4e));

    // --- PDPT level ---
    uint64_t pdpte = pdpt[idx.pdpt_index];
    if (!(pdpte & FLAGS_PRESENT)) {
        uint64_t *new_pd = allocate_page_table();
        uint64_t *new_pd_hhdm = add_hhdm_to(new_pd);
        if (!new_pd) panic("alloc_page: cannot allocate PD");
        for (int i = 0; i < 512; i++) new_pd_hhdm[i] = 0;
        pdpt[idx.pdpt_index] = ((uintptr_t)new_pd & PAGE_MASK)
                              | (flags & HIGHER_LEVEL_FLAGS)
                              | FLAGS_PRESENT;
        pdpte = pdpt[idx.pdpt_index];
    }
    uint64_t *pd = add_hhdm_to(page_table_to_address(pdpte));

    // --- PD level ---
    uint64_t pde = pd[idx.pd_index];
    if (!(pde & FLAGS_PRESENT)) {
        uint64_t *new_pt = allocate_page_table();
        uint64_t *new_pt_hhdm = add_hhdm_to(new_pt);
        if (!new_pt) panic("alloc_page: cannot allocate PT");
        for (int i = 0; i < 512; i++) new_pt_hhdm[i] = 0;
        pd[idx.pd_index] = ((uintptr_t)new_pt & PAGE_MASK)
                          | (flags & HIGHER_LEVEL_FLAGS)
                          | FLAGS_PRESENT;
        pde = pd[idx.pd_index];
    }
    uint64_t *pt = add_hhdm_to(page_table_to_address(pde));

    // --- PT level ---
    if (pt[idx.pt_index] & FLAGS_PRESENT) {
        panic("alloc_page: virtual 0x%p already mapped", (void*)vaddr);
    }

    pt[idx.pt_index] = (paddr & PAGE_MASK)
                      | flags
                      | FLAGS_PRESENT;

    return (void*)vaddr;
}

int free_page(void *page) {
    if (page == NULL) {
        return -1; // Nothing to free
    }

    uintptr_t address = (uintptr_t)page;
    page_address_t entry = get_page_entry(address);

    uint64_t* pml4 = (uint64_t*)pml4_address;
    if (!(pml4[entry.pml4_index] & FLAGS_PRESENT)) {
        return -1; // PML4 entry not present
    }

    uint64_t* pdpt = add_hhdm_to(page_table_to_address(pml4[entry.pml4_index]));
    if (!(pdpt[entry.pdpt_index] & FLAGS_PRESENT)) {
        return -1; // PDPT entry not present
    }

    uint64_t* pd = add_hhdm_to(page_table_to_address(pdpt[entry.pdpt_index]));
    if (!(pd[entry.pd_index] & FLAGS_PRESENT)) {
        return -1; // PD entry not present
    }

    uint64_t* pt = add_hhdm_to(page_table_to_address(pd[entry.pd_index]));
    if (!(pt[entry.pt_index] & FLAGS_PRESENT)) {
        return -1; // PT entry not present
    }

    // Free the page and clear the entry
    uintptr_t phys = pt[entry.pt_index] & PAGE_MASK;
    pt[entry.pt_index] = 0;

    // Invalidate the TLB for the virtual address
    asm volatile("invlpg (%0)" ::"r"(page) : "memory");

    // Remove allocation from bitmap
    size_t page_index = phys / PAGE_SIZE;
    memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));

    // Check if the PT, PD, or PDPT can be freed
    int empty = 1;
    for (int i = 0; i < 512; i++) {
        if (pt[i] & FLAGS_PRESENT) {
            empty = 0;
            break;
        }
    }
    if (empty) {
        size_t page_index = (uint64_t)page_table_to_address(pd[entry.pd_index]) / PAGE_SIZE;
        memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
        pd[entry.pd_index] = 0;
        asm volatile("invlpg (%0)" ::"r"(pd) : "memory");
    }

    empty = 1;
    for (int i = 0; i < 512; i++) {
        if (pd[i] & FLAGS_PRESENT) {
            empty = 0;
            break;
        }
    }
    if (empty) {
        size_t page_index = (uint64_t)page_table_to_address(pdpt[entry.pdpt_index]) / PAGE_SIZE;
        memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
        pdpt[entry.pdpt_index] = 0;
        asm volatile("invlpg (%0)" ::"r"(pdpt) : "memory");
    }

    empty = 1;
    for (int i = 0; i < 512; i++) {
        if (pdpt[i] & FLAGS_PRESENT) {
            empty = 0;
            break;
        }
    }
    if (empty) {
        size_t page_index = (uint64_t)page_table_to_address(pml4[entry.pml4_index]) / PAGE_SIZE;
        memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
        pml4[entry.pml4_index] = 0;
        asm volatile("invlpg (%0)" ::"r"(pml4) : "memory");
    }

    return 0; // Success
}

void* clone_page_tables(void* pml4_address) {
    uint64_t* new_pml4 = allocate_page_table();
    uint64_t* old_pml4 = add_hhdm_to(pml4_address);

    for (int i = 0; i < 512; i++) {
        uint64_t pml4e = old_pml4[i];

        if (i >= 256) {
            // Upper half (kernel space): share mappings
            add_hhdm_to(new_pml4)[i] = pml4e;
            continue;
        }

        if (pml4e & FLAGS_PRESENT) {
            uint64_t* pdpt = add_hhdm_to(page_table_to_address(pml4e));
            uint64_t* new_pdpt = allocate_page_table();

            for (int j = 0; j < 512; j++) {
                uint64_t pdpte = pdpt[j];
                if (pdpte & FLAGS_PRESENT) {
                    uint64_t* pd = add_hhdm_to(page_table_to_address(pdpte));
                    uint64_t* new_pd = allocate_page_table();

                    for (int k = 0; k < 512; k++) {
                        uint64_t pde = pd[k];
                        if (pde & FLAGS_PRESENT) {
                            uint64_t* pt = add_hhdm_to(page_table_to_address(pde));
                            uint64_t* new_pt = allocate_page_table();

                            for (int l = 0; l < 512; l++) {
                                if (pt[l] & FLAGS_PRESENT) {
                                    uintptr_t old_phys = pt[l] & PAGE_MASK;
                                    uintptr_t new_phys = get_available_address();

                                    // Directly copy contents using HHDM
                                    void* src = add_hhdm_to((uint64_t*)old_phys);
                                    void* dst = add_hhdm_to((uint64_t*)new_phys);
                                    memcpy(dst, src, PAGE_SIZE);

                                    // Mark the new page as allocated
                                    size_t page_index = new_phys / PAGE_SIZE;
                                    memory_bitmap[page_index / 8] |= (1 << (page_index % 8));

                                    // Point to new frame
                                    add_hhdm_to(new_pt)[l] = (new_phys & PAGE_MASK) | (pt[l] & FLAGS_MASK);
                                }
                            }
                            add_hhdm_to(new_pd)[k] = ((uintptr_t)new_pt & PAGE_MASK) | (pde & HIGHER_LEVEL_FLAGS);
                        }
                    }
                    add_hhdm_to(new_pdpt)[j] = ((uintptr_t)new_pd & PAGE_MASK) | (pdpte & HIGHER_LEVEL_FLAGS);
                }
            }
            add_hhdm_to(new_pml4)[i] = ((uintptr_t)new_pdpt & PAGE_MASK) | (pml4e & HIGHER_LEVEL_FLAGS);
        }
    }
    return new_pml4;
}

void free_page_tables(void* pml4_address) {
    uint64_t* pml4 = add_hhdm_to(pml4_address);
    for (int i = 0; i < 256; i++) { // Skip higher half entries
        uint64_t pml4e = pml4[i];
        if (pml4e & FLAGS_PRESENT) {
            uint64_t* pdpt = add_hhdm_to(page_table_to_address(pml4e));
            for (int j = 0; j < 512; j++) {
                uint64_t pdpte = pdpt[j];

                if (pdpte & FLAGS_PRESENT) {
                    uint64_t* pd = add_hhdm_to(page_table_to_address(pdpte));
                    for (int k = 0; k < 512; k++) {
                        uint64_t pde = pd[k];

                        if (pde & FLAGS_PRESENT) {
                            uint64_t* pt = add_hhdm_to(page_table_to_address(pde));

                            for (int l = 0; l < 512; l++) {
                                if (pt[l] & FLAGS_PRESENT) {
                                    uintptr_t phys = pt[l] & PAGE_MASK;

                                    size_t page_index = phys / PAGE_SIZE;
                                    memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
                                }
                            }
                            size_t page_index = (uint64_t)page_table_to_address(pde) / PAGE_SIZE;
                            memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
                        }
                    }
                    size_t page_index = (uint64_t)page_table_to_address(pdpte) / PAGE_SIZE;
                    memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
                }
            }
            size_t page_index = (uint64_t)page_table_to_address(pml4e) / PAGE_SIZE;
            memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
        }
    }
    size_t page_index = (uint64_t)pml4_address / PAGE_SIZE;
    memory_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
}

void change_pml4(void* pml4) {
    pml4_address = add_hhdm_to(pml4);
}
