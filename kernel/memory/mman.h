#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define ALLOC_MAP_SIZE 1024

void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size_per_element);
void* krealloc(void* ptr, size_t old_size, size_t new_size);
void* alloc_region(uintptr_t vaddr, size_t size, uint64_t flags);
void* alloc_mmio_region(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags);
void free_region(uintptr_t vaddr, size_t size);
void kfree(void* ptr);

void init_mman(size_t executable_size);

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* ptr, unsigned char value, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* ptr1, const void* ptr2, size_t n);
