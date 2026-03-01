#pragma once
#include <stdint.h>

typedef struct {
    uint16_t offset_low;    // Lower 16 bits of the offset to the handler
    uint16_t selector;      // Segment selector in the GDT
    uint8_t  ist;           // Interrupt Stack Table offset (usually 0)
    uint8_t  type_attr;     // Type and attributes (present, DPL, type)
    uint16_t offset_mid;    // Middle 16 bits of the offset to the handler
    uint32_t offset_high;   // Upper 32 bits of the offset to the handler
    uint32_t zero;          // Reserved
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;         // Size of the IDT in bytes
    uint64_t base;          // Base address of the IDT
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) iframe_t;

#define IDT_ENTRY_COUNT 256
#define CODE_SEGMENT_SELECTOR 0x8

void idt_set_entry(int vec, void (*isr)(), uint16_t selector, uint8_t type_attr, uint8_t ist);
void interrupt_handler(iframe_t* iframe);
void idt_load(idt_ptr_t* idt_ptr);
void idt_init();
