#pragma once
#include "console.h"
#include <stdarg.h>
#include "power.h"
#include "drivers/ps2_keyboard.h"

static inline __attribute__((noreturn)) void vpanic_int(uint64_t rbp, const char *fmt, va_list args) {
    clear_screen();

    // Set background color to black for panic messages
    setbg_color(COLOR(0, 0, 0));

    // Print the panic message
    kprintf(":( Kernel panic: ");
    kvprintf(fmt, args);
    kprintf("\n\n");

    // Stack trace
    kprintf("Stack trace:\n");
    if (rbp == 0) asm volatile("mov %%rbp, %0" : "=r"(rbp));
    for (int i = 0; ; i++) {
        uint64_t return_address = *((uint64_t*)(rbp + 8));
        if (return_address == 0) break;
        kprintf("  #%d: 0x%x\n", i, return_address);
        rbp = *((uint64_t*)rbp);
        if (rbp == 0) break;
    }

    kprintf("\nPress Enter to reboot...");

    char buffer[5];
    asm volatile ("sti");
    input_disabled = 0;
    get_input(buffer, 5, 1);
    reboot();
}

static inline __attribute__((noreturn)) void panic_int(uint64_t rbp, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vpanic_int(rbp, fmt, args);

    va_end(args);
}

static inline __attribute__((noreturn)) void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    uint64_t rbp;
    asm volatile("mov %%rbp, %0" : "=r"(rbp));
    vpanic_int(rbp, fmt, args);

    va_end(args);
}
