#include "memory/mman.h"
#include "memory/paging.h"
#include "util/string.h"
#include "panic.h"
#include <stddef.h>
#include <stdint.h>

uintptr_t kernel_heap_start;

typedef struct {
    uintptr_t start;
    int pages;
    uint8_t free;
} mapped_region;

mapped_region map[ALLOC_MAP_SIZE] = {0};
uint64_t map_index = 0;

void init_mman(size_t executable_size) {
    kernel_heap_start = PAGE_ALIGN(0xFFFFFFFF80000000ULL + executable_size);
    map[0].start = kernel_heap_start;
    map[0].pages = 0xFFFFFFFFF - (kernel_heap_start/PAGE_SIZE);
    map[0].free = 1;
    map_index = 1;
}

void kfree(void *ptr) {
    if ((uintptr_t)ptr < kernel_heap_start) {
        return; // Nothing to free
    }

    uint64_t pointer_map_index = 0;

    uintptr_t address = (uintptr_t)ptr;
    size_t size = 0;
    for (uint64_t i = 0; i < map_index; i++) {
        if (map[i].start == address) {
            pointer_map_index = i;
            size = map[i].pages * PAGE_SIZE;
        }
    }

    // Free the pages allocated for this pointer
    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        free_page((void*)(address + i));
    }

    // Mark the region as free
    map[pointer_map_index].free = 1;
}

void* find_available_va(size_t size) {
    for (uint64_t i = 0; i < map_index; i++) {
        if (map[i].free && map[i].pages >= size) {
            if (map[i].pages > size) {
                map[map_index].free = 1;
                map[map_index].start = map[i].start + size * PAGE_SIZE;
                map[map_index].pages = map[i].pages - size;

                map_index++;
                if (map_index >= ALLOC_MAP_SIZE) panic("Too many kernel allocations");
            }
            map[i].pages = size;
            map[i].free = 0;

            return (void*)(map[i].start);
        }
    }
    panic("Out of kernel heap addresses");
}

void* kmalloc(size_t size) {
    void* ptr = find_available_va(PAGE_ALIGN(size) / PAGE_SIZE);
    // Allocate memory of the specified size
    alloc_region((uintptr_t)ptr, size, FLAGS_PRESENT | FLAGS_RW);
    return ptr;
}

void* kzalloc(size_t size) {
    return memset(kmalloc(size), 0, size);
}

void* kcalloc(size_t num, size_t size_per_element) {
    return kzalloc(num * size_per_element);
}

void* krealloc(void* ptr, size_t old_size, size_t new_size) {
    if (ptr == NULL) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    uint8_t* size_count_ptr = ptr;

    void* new_ptr = kmalloc(new_size);

    if (new_ptr == NULL) {
        return NULL; // Allocation failed
    }

    memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    kfree(ptr);

    return new_ptr;
}
