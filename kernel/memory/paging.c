#include "paging.h"
#include "mman.h"
#include "../panic.h"
#include <stddef.h>
#include <stdint.h>

void* pml4_address = NULL;
void* hhdm_base = 0;
struct limine_memmap_response *memory_map = NULL;

used_region_t used_regions[1024] = {
    {0, 0, 0} // Initialize with 0s
};

int used_region_count = 0;

#define HIGHER_LEVEL_FLAGS (FLAGS_PRESENT | FLAGS_RW | FLAGS_USER)

void merge_used_regions() {
    // Merge adjacent used regions to avoid fragmentation
    for (int i = 0; i < used_region_count - 1; i++) {
        if (used_regions[i].max_addr == used_regions[i + 1].min_addr) {
            used_regions[i].max_addr = used_regions[i + 1].max_addr;
            for (int j = i + 1; j < used_region_count - 1; j++) {
                used_regions[j] = used_regions[j + 1];
            }
            used_region_count--;
            i--; // Check the current index again
        }
    }
}

int check_used(uintptr_t addr) {
    // Check if the address is already used in the used regions
    for (int i = 0; i < used_region_count; i++) {
        if (addr >= used_regions[i].min_addr && addr < used_regions[i].max_addr) {
            return 1; // Address is used
        }
    }
    return 0; // Address is not used
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
    // Initialize the paging system with the provided CR3 and memory map
    if (cr3 == 0 || memmap == NULL) {
        panic("init_paging failed: Invalid parameters: cr3 or memmap is NULL");
    }
    
    pml4_address = (void*)hhdm + cr3;
    hhdm_base = (void*)hhdm;
    memory_map = memmap;
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
    
    used_regions[used_region_count].min_addr = addr;
    used_regions[used_region_count].max_addr = addr + sizeof(uint64_t) * 512;
    used_regions[used_region_count].type = 1;
    used_region_count++;

    if (used_region_count >= 1024) {
        merge_used_regions();
    }

    if (used_region_count >= 1024) {
        panic("The buffer of used memory regions for paging has overflowed");
    }

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

    // record usage
    if (used_region_count >= 1024) {
        merge_used_regions();
        if (used_region_count >= 1024)
            panic("alloc_page: used_regions overflow");
    }
    used_regions[used_region_count++] = (used_region_t){
        .min_addr = phys,
        .max_addr = phys + PAGE_SIZE,
        .type     = 0
    };

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

    // Remove the used region
    for (int i = 0; i < used_region_count; i++) {
        if (used_regions[i].min_addr == phys && used_regions[i].max_addr == phys + PAGE_SIZE) {
            for (int j = i; j < used_region_count - 1; j++) {
                used_regions[j] = used_regions[j + 1];
            }
            used_region_count--;
            break;
        }
    }

    // Check if the PT, PD, or PDPT can be freed
    int empty = 1;
    for (int i = 0; i < 512; i++) {
        if (pt[i] & FLAGS_PRESENT) {
            empty = 0;
            break;
        }
    }
    if (empty) {
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

                                    // Add the used region for the new page
                                    used_regions[used_region_count].min_addr = new_phys;
                                    used_regions[used_region_count].max_addr = new_phys + PAGE_SIZE;
                                    used_regions[used_region_count].type = 0;
                                    used_region_count++;
                                    if (used_region_count >= 1024) {
                                        merge_used_regions();
                                        if (used_region_count >= 1024) {
                                            panic("clone_page_tables: Used regions overflow");
                                        }
                                    }

                                    // Point to new frame
                                    new_pt[l] = (new_phys & PAGE_MASK) | (uint64_t)page_table_to_address(pt[l]);
                                }
                            }
                            new_pd[k] = ((uintptr_t)new_pt & PAGE_MASK) | (pde & HIGHER_LEVEL_FLAGS);
                        }
                    }
                    new_pdpt[j] = ((uintptr_t)new_pd & PAGE_MASK) | (pdpte & HIGHER_LEVEL_FLAGS);
                }
            }
            add_hhdm_to(new_pml4)[i] = ((uintptr_t)new_pdpt & PAGE_MASK) | (pml4e & HIGHER_LEVEL_FLAGS);
        }
    }
    return new_pml4;
}

void change_pml4(void* pml4) {
    pml4_address = add_hhdm_to(pml4);
}