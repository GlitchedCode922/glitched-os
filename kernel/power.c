#include "power.h"
#include <stdint.h>

const uint16_t invalid_idtptr[5] = {0};

void __attribute__((noreturn)) reboot() {
    asm volatile (
        "lidt %0\n" // Load the IDT
        "int $0x00\n" // Send an interrupt to the CPU
        // This will triple fault the CPU, causing it to reboot
        :
        : "m"(invalid_idtptr) // Input: invalid IDT pointer
    );
    while (1) {
        // Infinite loop to prevent compiler warning
    }
}
