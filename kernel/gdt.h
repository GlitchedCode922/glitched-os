#pragma once
#include <stdint.h>

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

void gdt_init();
void gdt_flush();
