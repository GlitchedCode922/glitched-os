#include "break.h"
#include "scheduler.h"
#include "../memory/paging.h"
#include "../console.h"
#include <stdint.h>
#include <stddef.h>

#define USER_MAX 0x00007FFFFFFFFFFFULL

static inline uintptr_t page_align_up(uintptr_t addr) {
    return (addr + 0xFFF) & PAGE_MASK;
}

void* set_brk(void* addr) {
    if (current_task == NULL) return NULL;

    if (addr == NULL) return current_task->brk;

    uintptr_t old_brk = (uintptr_t)current_task->brk;
    uintptr_t new_brk = (uintptr_t)addr;
    uintptr_t initial_brk = (uintptr_t)current_task->initial_brk;

    if (new_brk < initial_brk) return NULL;
    if (new_brk > USER_MAX) return NULL;
    if (old_brk < initial_brk) return NULL;

    uintptr_t old_page_end = page_align_up(old_brk);
    uintptr_t new_page_end = page_align_up(new_brk);

    if (new_page_end > old_page_end) {
        for (uintptr_t page = old_page_end; page < new_page_end; page += 0x1000) {
            alloc_page(page, FLAGS_USER | FLAGS_RW);
        }
    } else if (new_page_end < old_page_end) {
        for (uintptr_t page = new_page_end; page < old_page_end; page += 0x1000) {
            free_page((void*)page);
        }
    }

    current_task->brk = addr;
    return addr;
}

void* sbrk(intptr_t increment) {
    if (current_task == NULL || current_task->brk == NULL) return (void*)-1;

    uintptr_t old_brk = (uintptr_t)current_task->brk;
    uintptr_t initial_brk = (uintptr_t)current_task->initial_brk;

    if (increment > 0) {
        uintptr_t amount = (uintptr_t)increment;

        if (amount > UINTPTR_MAX - old_brk) return (void*)-1;

        uintptr_t new_brk = old_brk + amount;

        if (new_brk > USER_MAX) return (void*)-1;
        if (set_brk((void*)new_brk) == NULL) return (void*)-1;
    } else if (increment < 0) {
        uintptr_t amount = (uintptr_t)(-(increment + 1)) + 1;

        if (old_brk < initial_brk || amount > old_brk - initial_brk) return (void*)-1;

        uintptr_t new_brk = old_brk - amount;

        if (set_brk((void*)new_brk) == NULL) return (void*)-1;
    }

    return (void*)old_brk;
}
