#include <stdint.h>
#include "io/ports.h"
#include "io/ps2.h"
#include "io/8259pic.h"
#include "drivers/ps2_keyboard.h"
#include "drivers/ps2_mouse.h"

static int dual_channel = 0;
static int mouse_present = 0;

void ps2_init() {
    irq_handlers[1].arg = 0;
    irq_handlers[1].handler = ps2_handler;
    irq_handlers[12].arg = (void*)1;
    irq_handlers[12].handler = ps2_handler;
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
    if (dual_channel) outb(0x64, 0xA8);
    outb(0x64, 0x20); // Read command byte
    while (!(inb(0x64) & 0x01));
    command_byte = inb(0x60);
    outb(0x64, 0x60);
    while (inb(0x64) & 0x02);
    outb(0x60, command_byte | 0x01 | (dual_channel ? 0x02 : 0));
    ps2_keyboard_init();
    // Check for a mouse
    if (!dual_channel) return;
    outb(0x64, 0xD4);
    outb(0x60, 0xF2); // Identify command
    while (!(inb(0x64) & 0x01));
    if (inb(0x60) != 0xFA) return;
    while (!(inb(0x64) & 0x01));
    char mouse_id = inb(0x60);
    if (mouse_id == 0x00) {
        mouse_present = 1;
        ps2_mouse_init();
    }
}

uint32_t ps2_handler(void* aux) {
    // Read the status register
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        // Data is available
        uint8_t data = inb(0x60);
        if (aux) {
            // Data from the 2nd PS/2 port
            if (!mouse_present) return 0;
            ps2_mouse_handler(data);
        } else {
            // Data from the 1st PS/2 port
            ps2_keyboard_handler(data);
        }
    }
    return 0;
}
