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
    kprintf("\n\nPress Enter to reboot...");

    get_input_line();
    reboot();

    va_end(args);
}
