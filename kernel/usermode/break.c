#include "break.h"
#include "scheduler.h"
#include "../memory/paging.h"
#include <stddef.h>

void* set_brk(void* addr) {
    if (addr == NULL) {
        return current_task->brk; // Return current break if addr is NULL
    }

    // Enforce address is canonical and above initial break
    if ((uintptr_t)addr >= 0x0000800000000000 && (uintptr_t)addr < 0xFFFF800000000000 && (uintptr_t)addr >= (uintptr_t)current_task->initial_brk) {
        // Valid address
    } else {
        return NULL;
    }

    if (current_task->brk < current_task->initial_brk) {
        return NULL; // Current break is below initial break, should not happen
    }

    // Check if the new break is below the current break
    if (current_task->brk != NULL && addr < current_task->brk) {
        // Get page difference and unmap pages
        uintptr_t current_page = (uintptr_t)current_task->brk & ~0xFFF;
        uintptr_t new_page = (uintptr_t)addr & ~0xFFF;
        while (current_page > new_page) {
            free_page((void*)current_page);
            current_page -= 0x1000; // Move to the previous page
        }
    }

    if (current_task->brk != NULL && addr > current_task->brk) {
        // Get page difference and allocate pages
        uintptr_t current_page = (uintptr_t)current_task->brk & ~0xFFF;
        uintptr_t new_page = (uintptr_t)addr & ~0xFFF;
        while (current_page < new_page) {
            alloc_page(current_page, FLAGS_USER | FLAGS_RW);
            current_page += 0x1000; // Move to the next page
        }
    }

    if (addr == NULL) {
        return current_task->brk; // Return current break if addr is NULL
    }

    current_task->brk = addr;
    return current_task->brk;
}

void* sbrk(intptr_t increment) {
    if (current_task->brk == NULL) {
        return NULL;
    }
    void* new_brk = (void*)((uintptr_t)current_task->brk + increment);
    if (set_brk(new_brk) == NULL) {
        return NULL;
    }
    return current_task->brk;
}
