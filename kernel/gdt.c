#include "gdt.h"
#include "memory/mman.h"
#include <stdint.h>

#define GDT_ENTRY_COUNT 7
uint64_t gdt[GDT_ENTRY_COUNT];
tss_t tss = {0};

void gdt_init() {
    gdt[0] = 0;

    uint64_t kernel_code = 0;
    kernel_code |= 0b1011 << 8; // type of selector
    kernel_code |= 1 << 12; // not a system descriptor
    kernel_code |= 0 << 13; // DPL field = 0
    kernel_code |= 1 << 15; // present
    kernel_code |= 1 << 21; // long-mode segment

    gdt[1] = kernel_code << 32;

    uint64_t kernel_data = 0;
    kernel_data |= 0b0011 << 8; // type of selector
    kernel_data |= 1 << 12; // not a system descriptor
    kernel_data |= 0 << 13; // DPL field = 0
    kernel_data |= 1 << 15; // present
    kernel_data |= 1 << 21; // long-mode segment

    gdt[2] = kernel_data << 32;

    uint64_t user_code = kernel_code | (3 << 13);
    gdt[3] = user_code << 32;

    uint64_t user_data = kernel_data | (3 << 13);
    gdt[4] = user_data << 32;

    uint64_t tss_low = 0;
    tss_low |= (sizeof(tss) - 1) & 0xFFFF; // limit
    tss_low |= ((uint64_t)&tss & 0xFFFFFF) << 16; // base 0-23
    tss_low |= (uint64_t)0x9 << 40; // type = TSS, DPL = 0
    tss_low |= 1ULL << 47; // present
    tss_low |= ((sizeof(tss) - 1) & 0xF0000) << 32; // limit high
    tss_low |= (((uint64_t)&tss >> 24) & 0xFF) << 56; // base 24-31
    uint64_t tss_high = ((uint64_t)&tss >> 32) & 0xFFFFFFFF; // base 32-63

    gdt[5] = tss_low;
    gdt[6] = tss_high;

    tss.rsp0 = 0;

    gdt_flush();
}

gdt_ptr_t gdt_ptr = {
    .limit = GDT_ENTRY_COUNT * sizeof(uint64_t) - 1,
    .base = (uint64_t)gdt,
};

void gdt_flush() {
    asm volatile("\
        lgdt %0 \n\
        mov $0x10, %%ax \n\
        mov %%ax, %%ds \n\
        mov %%ax, %%es \n\
        mov %%ax, %%fs \n\
        mov %%ax, %%gs \n\
        mov %%ax, %%ss \n\
        mov $0x28, %%ax \n\
        ltr %%ax \n\
        \n\
        pushq $0x8\n\
        lea 1f(%%rip), %%rax\n\
        push %%rax\n\
        lretq\n\
        1:"
        : : "m"(gdt_ptr) : "rax", "memory" );
}

void set_rsp0(uint64_t rsp) {
    tss.rsp0 = rsp;
}
