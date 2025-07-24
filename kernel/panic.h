#pragma once
#include "console.h"
#include <stdarg.h>

static inline __attribute__((noreturn)) void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    clear_screen();
    
    // Set background color to black for panic messages
    setbg_color(COLOR(0, 0, 0));
    
    // Print the panic message
    kprintf(":( Kernel panic: ");
    kvprintf(fmt, args);
    
    while (1) {
        asm volatile("hlt");
    }
    
    va_end(args);
}
