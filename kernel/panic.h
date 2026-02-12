#pragma once
#include "console.h"
#include <stdarg.h>
#include "power.h"
#include "drivers/ps2_keyboard.h"

static inline __attribute__((noreturn)) void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    clear_screen();

    // Set background color to black for panic messages
    setbg_color(COLOR(0, 0, 0));

    // Print the panic message
    kprintf(":( Kernel panic: ");
    kvprintf(fmt, args);
    kprintf("\n\n");

    // Stack trace
    uint64_t rbp;
    asm volatile("mov %%rbp, %0" : "=r"(rbp));
    kprintf("Stack trace:\n");
    for (int i = 0; ; i++) {
        uint64_t return_address = *((uint64_t*)(rbp + 8));
        if (return_address == 0) break;
        kprintf("  #%d: 0x%x\n", i, return_address);
        rbp = *((uint64_t*)rbp);
        if (rbp == 0) break;
    }

    kprintf("\nPress Enter to reboot...");

    char buffer[5];
    get_input(buffer, 5, 1);
    reboot();

    va_end(args);
}
