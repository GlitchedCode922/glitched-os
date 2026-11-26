#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define ALLOC_MAP_SIZE 8192

void* kmalloc(size_t size);
void* kzalloc(size_t size);
void* kcalloc(size_t num, size_t size_per_element);
void* krealloc(void* ptr, size_t old_size, size_t new_size);
void kfree(void* ptr);
void* find_available_va(size_t size);

void mman_init(size_t executable_size);
