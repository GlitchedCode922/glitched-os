#include "mman.h"
#include "paging.h"
#include "../panic.h"
#include <stddef.h>
#include <stdint.h>

uintptr_t kernel_heap_start;

typedef struct {
    uintptr_t start;
    int pages;
    uint8_t free;
} mapped_region;

mapped_region map[1024] = {0};
uint64_t map_index = 0;

void init_mman(size_t executable_size) {
    kernel_heap_start = PAGE_ALIGN(0xFFFFFFFF80000000ULL + executable_size);
    map[0].start = kernel_heap_start;
    map[0].pages = 0xFFFFFFFFF - (kernel_heap_start/PAGE_SIZE);
    map[0].free = 1;
    map_index = 1;
}

int memcmp(const void *ptr1, const void *ptr2, size_t n){
    const unsigned char *p1 = (const unsigned char *)ptr1;
    const unsigned char *p2 = (const unsigned char *)ptr2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0; // Memory regions are equal
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* ptr, unsigned char value, size_t n) {
    unsigned char *p = (unsigned char *)ptr;

    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }
    return ptr;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s || d >= s + n) {
        // Non-overlapping regions
        return memcpy(dest, src, n);
    } else {
        // Overlapping regions
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
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
    for (int i = 0; i <= map_index; i++) {
        if (map[i].free && map[i].pages >= size) {
            map[map_index].free = 1;
            map[map_index].start = map[i].start + size * PAGE_SIZE;
            map[map_index].pages = map[i].pages - size;
            map[i].pages = size;
            map[i].free = 0;

            map_index++;

            return (void*)(map[i].start);
        }
    }
    panic("Out of kernel heap addresses");
}

void* kmalloc(size_t size) {
    void* ptr = find_available_va(PAGE_ALIGN(size) / PAGE_SIZE);
    // Allocate memory of the specified size
    for (int i=0; i < PAGE_ALIGN(size); i += PAGE_SIZE) {
        alloc_page((uintptr_t)ptr + i, FLAGS_PRESENT | FLAGS_RW);
    }
    memset(ptr, 0, size); // Initialize allocated memory to zero
    return ptr;
}

void* kcalloc(size_t num, size_t size_per_element) {
    return kmalloc(num * size_per_element);
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
