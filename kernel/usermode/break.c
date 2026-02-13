#include "break.h"
#include "../memory/paging.h"
#include <stddef.h>

void* initial_brk = NULL;
void* brk = NULL;

void* set_brk(void* addr) {
    if (addr == NULL) {
        return brk; // Return current break if addr is NULL
    }

    // Enforce address is canonical and above initial break
    if ((uintptr_t)addr >= 0x0000800000000000 && (uintptr_t)addr < 0xFFFF800000000000 && (uintptr_t)addr >= (uintptr_t)initial_brk) {
        // Valid address
    } else {
        return NULL;
    }

    if (brk < initial_brk) {
        return NULL; // Current break is below initial break, should not happen
    }

    // Check if the new break is below the current break
    if (brk != NULL && addr < brk) {
        // Get page difference and unmap pages
        uintptr_t current_page = (uintptr_t)brk & ~0xFFF;
        uintptr_t new_page = (uintptr_t)addr & ~0xFFF;
        while (current_page > new_page) {
            free_page((void*)current_page);
            current_page -= 0x1000; // Move to the previous page
        }
    }

    if (brk != NULL && addr > brk) {
        // Get page difference and allocate pages
        uintptr_t current_page = (uintptr_t)brk & ~0xFFF;
        uintptr_t new_page = (uintptr_t)addr & ~0xFFF;
        while (current_page < new_page) {
            alloc_page(current_page, FLAGS_USER | FLAGS_RW);
            current_page += 0x1000; // Move to the next page
        }
    }

    if (addr == NULL) {
        return brk; // Return current break if addr is NULL
    }

    brk = addr;
    return brk;
}

void* sbrk(intptr_t increment) {
    if (brk == NULL) {
        return NULL;
    }
    void* new_brk = (void*)((uintptr_t)brk + increment);
    if (set_brk(new_brk) == NULL) {
        return NULL;
    }
    return brk;
}
