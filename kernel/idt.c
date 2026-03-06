#include "idt.h"
#include "io/8259pic.h"
#include "drivers/timer.h"
#include "panic.h"
#include "console.h"
#include "drivers/ps2_keyboard.h"
#include "usermode/scheduler.h"
#include "usermode/syscalls.h"
#include "memory/paging.h"
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

const char* exception_names[32] = {
    "Division by zero",               // 0
    "Debugging trap",                 // 1
    "Non-maskable interrupt",         // 2
    "Breakpoint",                     // 3
    "Overflow",                       // 4
    "Bound range exceeded",           // 5
    "Invalid opcode",                 // 6
    "Device not available",           // 7
    "Double fault",                   // 8
    "Unhandled exception",            // 9
    "Invalid TSS",                    // 10
    "Segment not present exception",  // 11
    "Stack-segment fault",            // 12
    "General protection fault",       // 13
    "Page fault",                     // 14
    "Unhandled exception",            // 15
    "x87 floating-point exception",   // 16
    "Alignment error",                // 17
    "Hardware error",                 // 18
    "SIMD floating-point exception",  // 19
    "Virtualization exception",       // 20
    "Control protection exception",   // 21
    "Unhandled exception",            // 22
    "Unhandled exception",            // 23
    "Unhandled exception",            // 24
    "Unhandled exception",            // 25
    "Unhandled exception",            // 26
    "Unhandled exception",            // 27
    "Hypervisor injection exception", // 28
    "VMM communication exception",    // 29
    "Security exception",             // 30
    "Unhandled exception"             // 31
};

idt_entry_t idt[IDT_ENTRY_COUNT];
idt_ptr_t idt_ptr = {
    .limit = sizeof(idt) - 1,
    .base = (uint64_t)&idt
};

void decode_pfec_flags(uint64_t error_code, char buffer[128]) {
    int pos = 0;

    #define APPEND(str) do { \
        const char* s = str; \
        while (*s && pos < 127) { \
            buffer[pos++] = *s++; \
        } \
    } while (0)

    #define NEWLINE() do { if (pos < 127) buffer[pos++] = '\n'; } while(0)

    if (error_code & 0x1) {
        APPEND(" - Page present: Yes");
    } else {
        APPEND(" - Page present: No");
    }
    NEWLINE();

    if (error_code & 0x2) {
        APPEND(" - Write access");
    } else {
        APPEND(" - Read access");
    }
    NEWLINE();

    if (error_code & 0x4) {
        APPEND(" - User mode");
    } else {
        APPEND(" - Kernel mode");
    }
    NEWLINE();

    if (error_code & 0x8) {
        APPEND(" - Reserved violation");
        NEWLINE();
    }

    if (error_code & 0x10) {
        APPEND(" - Instruction fetch");
        NEWLINE();
    }

    buffer[pos] = '\0';

    #undef APPEND
    #undef NEWLINE
}

static int strcmp(char* p1, char* p2) {
    while (*p1 && *p2 && *p1 == *p2) {
        p1++;
        p2++;
    };
    return (unsigned char)*p1 - (unsigned char)*p2;
}

void breakpoint_debugger(iframe_t* iframe) {
    kprintf("Breakpoint hit at 0x%x\n", iframe->rip - 1);
    char buffer[4096];
    asm volatile ("sti");
    termios_t term = keyboard_tty.termios;
    keyboard_tty.termios.c_lflag = ICANON | ECHO | ECHOE;
    while (1) {
        kprintf("> ");
        int bytes_read = tty_read(&keyboard_tty, buffer, sizeof(buffer) - 1, 1);
        buffer[bytes_read] = '\0'; // Null-terminate
        if (strcmp(buffer, "?\n") == 0 || strcmp(buffer, "help\n") == 0) {
            kprintf("Commands:\n");
            kprintf(" ? / help: Print this message\n");
            kprintf(" bt: Print a stack trace\n");
            kprintf(" continue: Continue execution\n");
        } else if (strcmp(buffer, "bt\n") == 0) {
            stack_trace(iframe->rbp);
        } else if (strcmp(buffer, "continue\n") == 0) {
            // Skip consecutive breakpoints
            while (*(uint8_t*)(iframe->rip) == 0xCC) iframe->rip++;
            keyboard_tty.termios = term;
            return;
        } else {
            kprintf("Invalid command, enter ? or help to see a list of commands\n");
        }
    }
}

void interrupt_handler(iframe_t* iframe) {
    uint64_t vector = iframe->vector;
    uint64_t error_code = iframe->error_code;
    if (vector == 14) {
        // Page fault
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        if (error_code & 0x2 && cow_handler((void*)(cr2 & PAGE_MASK))) return; // CoW page
        char flags[128] = {0};
        decode_pfec_flags(error_code, flags);
        if (iframe->cs == USER_CS) {
            kprintf("Page fault in process with PID %d at address 0x%x, error code: 0x%x\n%s", current_task->pid, cr2, error_code, flags);
            exit(-vector);
        } else {
            panic_int(iframe->rbp, "Page fault in kernel at address: 0x%x, error code: 0x%x\n%s", cr2, error_code, flags);
        }
    } else if (vector == 13) {
        // General protection fault
        if (iframe->cs == USER_CS) {
            kprintf("#GP in process with PID %d, error code: 0x%x\n", current_task->pid, error_code);
            exit(-vector);
        } else {
            panic_int(iframe->rbp, "#GP in kernel, error code: 0x%x\n", error_code);
        }
    } else if (vector == 3) {
        breakpoint_debugger(iframe);
    } else if (vector == 18 || vector == 2 || vector == 10 || vector == 11 || vector == 8 || vector == 28 || vector == 29) { // Serious errors
        panic_int(iframe->rbp, "%s with error code: 0x%x\n", exception_names[vector], error_code);
    } else if (vector < 32) {
        if (iframe->cs == USER_CS) {
            kprintf("%s in process with PID %d, error code: 0x%x\n", exception_names[vector], current_task->pid, error_code);
            exit(-vector);
        } else {
            panic_int(iframe->rbp, "%s in kernel, error code: 0x%x\n", exception_names[vector], error_code);
        }
    } else if (vector == 128) {
        syscall(iframe->rax, iframe->r12, iframe->rsi, iframe->rdx, iframe->r10, iframe->r8, iframe);
        if (ticks_remaining <= 0 && iframe->cs == USER_CS) {
            run_next(iframe); // Next task
        }
    } else {
        irq_handler(vector - 32, iframe);
    }
}

void idt_set_entry(int vec, void (*isr)(), uint16_t selector, uint8_t type_attr, uint8_t ist){
    uint64_t isr_address = (uint64_t)isr;
    idt[vec].offset_low = isr_address & 0xFFFF;
    idt[vec].selector = selector;
    idt[vec].ist = ist;
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
    idt_set_entry(0, isr0, KERNEL_CS, 0x8E, 0);
    idt_set_entry(1, isr1, KERNEL_CS, 0x8E, 0);
    idt_set_entry(2, isr2, KERNEL_CS, 0x8E, 0);
    idt_set_entry(3, isr3, KERNEL_CS, 0xEE, 0);
    idt_set_entry(4, isr4, KERNEL_CS, 0x8E, 0);
    idt_set_entry(5, isr5, KERNEL_CS, 0x8E, 0);
    idt_set_entry(6, isr6, KERNEL_CS, 0x8E, 0);
    idt_set_entry(7, isr7, KERNEL_CS, 0x8E, 0);
    idt_set_entry(8, isr8, KERNEL_CS, 0x8E, 2);
    idt_set_entry(9, isr9, KERNEL_CS, 0x8E, 0);
    idt_set_entry(10, isr10, KERNEL_CS, 0x8E, 0);
    idt_set_entry(11, isr11, KERNEL_CS, 0x8E, 0);
    idt_set_entry(12, isr12, KERNEL_CS, 0x8E, 0);
    idt_set_entry(13, isr13, KERNEL_CS, 0x8E, 0);
    idt_set_entry(14, isr14, KERNEL_CS, 0x8E, 1);
    idt_set_entry(15, isr15, KERNEL_CS, 0x8E, 0);
    idt_set_entry(16, isr16, KERNEL_CS, 0x8E, 0);
    idt_set_entry(17, isr17, KERNEL_CS, 0x8E, 0);
    idt_set_entry(18, isr18, KERNEL_CS, 0x8E, 1);
    idt_set_entry(19, isr19, KERNEL_CS, 0x8E, 0);
    idt_set_entry(20, isr20, KERNEL_CS, 0x8E, 0);
    idt_set_entry(21, isr21, KERNEL_CS, 0x8E, 0);
    idt_set_entry(22, isr22, KERNEL_CS, 0x8E, 0);
    idt_set_entry(23, isr23, KERNEL_CS, 0x8E, 0);
    idt_set_entry(24, isr24, KERNEL_CS, 0x8E, 0);
    idt_set_entry(25, isr25, KERNEL_CS, 0x8E, 0);
    idt_set_entry(26, isr26, KERNEL_CS, 0x8E, 0);
    idt_set_entry(27, isr27, KERNEL_CS, 0x8E, 0);
    idt_set_entry(28, isr28, KERNEL_CS, 0x8E, 0);
    idt_set_entry(29, isr29, KERNEL_CS, 0x8E, 0);
    idt_set_entry(30, isr30, KERNEL_CS, 0x8E, 0);
    idt_set_entry(31, isr31, KERNEL_CS, 0x8E, 0);
    idt_set_entry(32, isr32, KERNEL_CS, 0x8E, 0);
    idt_set_entry(33, isr33, KERNEL_CS, 0x8E, 0);
    idt_set_entry(34, isr34, KERNEL_CS, 0x8E, 0);
    idt_set_entry(35, isr35, KERNEL_CS, 0x8E, 0);
    idt_set_entry(36, isr36, KERNEL_CS, 0x8E, 0);
    idt_set_entry(37, isr37, KERNEL_CS, 0x8E, 0);
    idt_set_entry(38, isr38, KERNEL_CS, 0x8E, 0);
    idt_set_entry(39, isr39, KERNEL_CS, 0x8E, 0);
    idt_set_entry(40, isr40, KERNEL_CS, 0x8E, 0);
    idt_set_entry(41, isr41, KERNEL_CS, 0x8E, 0);
    idt_set_entry(42, isr42, KERNEL_CS, 0x8E, 0);
    idt_set_entry(43, isr43, KERNEL_CS, 0x8E, 0);
    idt_set_entry(44, isr44, KERNEL_CS, 0x8E, 0);
    idt_set_entry(45, isr45, KERNEL_CS, 0x8E, 0);
    idt_set_entry(46, isr46, KERNEL_CS, 0x8E, 0);
    idt_set_entry(47, isr47, KERNEL_CS, 0x8E, 0);
    idt_set_entry(128, isr128, KERNEL_CS, 0xEE, 0);
    // Load the IDT
    idt_load(&idt_ptr);
    // Enable interrupts
    asm volatile("sti");
}
