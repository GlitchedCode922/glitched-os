#include "idt.h"
#include "panic.h"
#include "console.h"
#include <stdint.h>

// ISR handlers (defined in assembly)
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
// End of ISR handlers

idt_entry_t idt[IDT_ENTRY_COUNT];
idt_ptr_t idt_ptr = {
    .limit = sizeof(idt) - 1,
    .base = (uint64_t)&idt
};

void interrupt_handler(uint64_t* stack) {
    uint64_t vector = stack[15];
    uint64_t error_code = stack[14];
    if (vector == 14) {
        // Page fault
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        panic("Page fault at address: 0x%x, error code: 0x%x\n", cr2, error_code);
    } else {
        panic("Unhandled interrupt: vector %d, error code: 0x%x\n", vector, error_code);
    }
}

void idt_set_entry(int vec, void (*isr)(), uint16_t selector, uint8_t type_attr){
    uint64_t isr_address = (uint64_t)isr;
    idt[vec].offset_low = isr_address & 0xFFFF;
    idt[vec].selector = selector;
    idt[vec].ist = 0; // IST, set to 0
    idt[vec].type_attr = type_attr;
    idt[vec].offset_mid = (isr_address >> 16) & 0xFFFF;
    idt[vec].offset_high = (isr_address >> 32) & 0xFFFFFFFF;
    idt[vec].zero = 0; // Reserved, set to 0
}

void idt_load(idt_ptr_t* idt_ptr) {
    asm volatile("lidt (%0)" : : "r"(idt_ptr));
}

void idt_init() {
    // Initialize IDT entries for each ISR
    idt_set_entry(0, isr0, 0x28, 0x8E);
    idt_set_entry(1, isr1, 0x28, 0x8E);
    idt_set_entry(2, isr2, 0x28, 0x8E);
    idt_set_entry(3, isr3, 0x28, 0x8E);
    idt_set_entry(4, isr4, 0x28, 0x8E);
    idt_set_entry(5, isr5, 0x28, 0x8E);
    idt_set_entry(6, isr6, 0x28, 0x8E);
    idt_set_entry(7, isr7, 0x28, 0x8E);
    idt_set_entry(8, isr8, 0x28, 0x8E);
    idt_set_entry(9, isr9, 0x28, 0x8E);
    idt_set_entry(10, isr10, 0x28, 0x8E);
    idt_set_entry(11, isr11, 0x28, 0x8E);
    idt_set_entry(12, isr12, 0x28, 0x8E);
    idt_set_entry(13, isr13, 0x28, 0x8E);
    idt_set_entry(14, isr14, 0x28, 0x8E);
    idt_set_entry(15, isr15, 0x28, 0x8E);
    idt_set_entry(16, isr16, 0x28, 0x8E);
    idt_set_entry(17, isr17, 0x28, 0x8E);
    idt_set_entry(18, isr18, 0x28, 0x8E);
    idt_set_entry(19, isr19, 0x28, 0x8E);
    idt_set_entry(20, isr20, 0x28, 0x8E);
    idt_set_entry(21, isr21, 0x28, 0x8E);
    idt_set_entry(22, isr22, 0x28, 0x8E);
    idt_set_entry(23, isr23, 0x28, 0x8E);
    idt_set_entry(24, isr24, 0x28, 0x8E);
    idt_set_entry(25, isr25, 0x28, 0x8E);
    idt_set_entry(26, isr26, 0x28, 0x8E);
    idt_set_entry(27, isr27, 0x28, 0x8E);
    idt_set_entry(28, isr28, 0x28, 0x8E);
    idt_set_entry(29, isr29, 0x28, 0x8E);
    idt_set_entry(30, isr30, 0x28, 0x8E);
    idt_set_entry(31, isr31, 0x28, 0x8E);
    // Load the IDT
    idt_load(&idt_ptr);
    // Enable interrupts
    asm volatile("sti");
}
