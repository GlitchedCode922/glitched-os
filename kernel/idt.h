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

#define IDT_ENTRY_COUNT 256

void idt_set_entry(int vec, void (*isr)(), uint16_t selector, uint8_t type_attr);
void interrupt_handler(uint64_t* stack);
void idt_load(idt_ptr_t* idt_ptr);
void idt_init();
