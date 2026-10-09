#include <stdint.h>
#include "io/ports.h"

static int dual_channel = 0;

void ps2_init() {
    // Disable both devices
    outb(0x64, 0xAD);
    outb(0x64, 0xA7);
    inb(0x60); // Discard any data in the buffer
    outb(0x64, 0x20); // Read command byte
    while (!(inb(0x64) & 0x01));
    uint8_t command_byte = inb(0x60);
    outb(0x64, 0x60); // Write command byte
    while (inb(0x64) & 0x02);
    outb(0x60, command_byte & ~(0x01 | 0x10) | 0x40);
    outb(0x64, 0xAA); // Self-test
    while (!(inb(0x64) & 0x01));
    if (inb(0x60) != 0x55) return; // Self-test failed
    // Check if second PS/2 port is available
    outb(0x64, 0xA8); // Enable second PS/2 port
    outb(0x64, 0x20); // Read command byte
    while (!(inb(0x64) & 0x01));
    command_byte = inb(0x60);
    if (command_byte & 0x20) {
        // Second PS/2 port is not available
    } else {
        // Second PS/2 port is available
        dual_channel = 1;
        outb(0x64, 0xA7);
        outb(0x64, 0x20); // Read command byte
        while (!(inb(0x64) & 0x01));
        command_byte = inb(0x60);
        outb(0x64, 0x60);
        while (inb(0x64) & 0x02);
        outb(0x60, command_byte & ~0x22);
    }
    // Enable both devices
    outb(0x64, 0xAE);
    //if (dual_channel) outb(0x64, 0xA8);
    outb(0x64, 0x20); // Read command byte
    while (!(inb(0x64) & 0x01));
    command_byte = inb(0x60);
    outb(0x64, 0x60);
    while (inb(0x64) & 0x02);
    outb(0x60, command_byte | 0x01 | (dual_channel ? 0x02 : 0));
}
