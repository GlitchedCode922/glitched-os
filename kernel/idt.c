#include "idt.h"
#include "io/8259pic.h"
#include "drivers/timer.h"
#include "panic.h"
#include "console.h"
#include "usermode/syscalls.h"
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
extern void isr32();
extern void isr33();
extern void isr34();
extern void isr35();
extern void isr36();
extern void isr37();
extern void isr38();
extern void isr39();
extern void isr40();
extern void isr41();
extern void isr42();
extern void isr43();
extern void isr44();
extern void isr45();
extern void isr46();
extern void isr47();
extern void isr128(); // System call handler
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
    } else if (vector < 32) {
        panic("Unhandled exception: vector %d, error code: 0x%x\n", vector, error_code);
    } else if (vector == 128) {
        uint64_t syscall_number, arg1, arg2, arg3, arg4, arg5;
        asm volatile(
            "mov %%rax, %0\n"
            "mov %%r12, %1\n"
            "mov %%rsi, %2\n"
            "mov %%rdx, %3\n"
            "mov %%r10, %4\n"
            "mov %%r8, %5\n"
            : "=r"(syscall_number), "=r"(arg1), "=r"(arg2), "=r"(arg3), "=r"(arg4), "=r"(arg5)
        );
        uint64_t result = syscall(syscall_number, arg1, arg2, arg3, arg4, arg5);
        asm volatile("mov %0, %%rax" : : "r"(result));
    } else {
        irq_handler(vector - 32);
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
    timer_init();
    pic_init();
    // Initialize IDT entries for each ISR
    idt_set_entry(0, isr0, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(1, isr1, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(2, isr2, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(3, isr3, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(4, isr4, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(5, isr5, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(6, isr6, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(7, isr7, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(8, isr8, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(9, isr9, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(10, isr10, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(11, isr11, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(12, isr12, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(13, isr13, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(14, isr14, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(15, isr15, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(16, isr16, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(17, isr17, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(18, isr18, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(19, isr19, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(20, isr20, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(21, isr21, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(22, isr22, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(23, isr23, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(24, isr24, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(25, isr25, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(26, isr26, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(27, isr27, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(28, isr28, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(29, isr29, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(30, isr30, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(31, isr31, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(32, isr32, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(33, isr33, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(34, isr34, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(35, isr35, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(36, isr36, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(37, isr37, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(38, isr38, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(39, isr39, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(40, isr40, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(41, isr41, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(42, isr42, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(43, isr43, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(44, isr44, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(45, isr45, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(46, isr46, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(47, isr47, CODE_SEGMENT_SELECTOR, 0x8E);
    idt_set_entry(128, isr128, CODE_SEGMENT_SELECTOR, 0x8E);
    // Load the IDT
    idt_load(&idt_ptr);
    // Enable interrupts
    asm volatile("sti");
}
