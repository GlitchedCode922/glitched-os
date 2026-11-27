#include <stdint.h>
#include "io/ports.h"
#include "io/ps2.h"
#include "io/8259pic.h"
#include "drivers/ps2_keyboard.h"
#include "drivers/ps2_mouse.h"
#include "util/spin.h"
#include "util/log.h"

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
    if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller, command timed out\n");
        return;
    }
    uint8_t command_byte = inb(0x60);
    outb(0x64, 0x60); // Write command byte
    if (spin(inb(0x64) & 0x02, 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
        return;
    }
    outb(0x60, command_byte & ~(0x01 | 0x10) | 0x40);
    outb(0x64, 0xAA); // Self-test
    if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
        return;
    }
    if (inb(0x60) != 0x55) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (self-test failed)\n");
        return;
    }
    // Check if second PS/2 port is available
    outb(0x64, 0xA8); // Enable second PS/2 port
    outb(0x64, 0x20); // Read command byte
    if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (self-test failed)\n");
        return;
    }
    command_byte = inb(0x60);
    if (command_byte & 0x20) {
        // Second PS/2 port is not available
        klog(LOG_INFO, "ps2", "Found single-channel PS/2 controller\n");
    } else {
        // Second PS/2 port is available
        klog(LOG_INFO, "ps2", "Found dual-channel PS/2 controller\n");
        dual_channel = 1;
        outb(0x64, 0xA7);
        outb(0x64, 0x20); // Read command byte
        if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
            klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
            return;
        }
        command_byte = inb(0x60);
        outb(0x64, 0x60);
        if (spin(inb(0x64) & 0x02, 500,) < 0) {
            klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
            return;
        }
        outb(0x60, command_byte & ~0x22);
    }
    // Enable both devices
    outb(0x64, 0xAE);
    if (dual_channel) outb(0x64, 0xA8);
    outb(0x64, 0x20); // Read command byte
    if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
        return;
    }
    command_byte = inb(0x60);
    outb(0x64, 0x60);
    if (spin(inb(0x64) & 0x02, 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
        return;
    }
    outb(0x60, command_byte | 0x01 | (dual_channel ? 0x02 : 0));
    ps2_keyboard_init();
    // Check for a mouse
    if (!dual_channel) return;
    outb(0x64, 0xD4);
    outb(0x60, 0xF2); // Identify command
    if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
        return;
    }
    if (inb(0x60) != 0xFA) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (received invalid command response)\n");
        return;
    }
    if (spin(!(inb(0x64) & 0x01), 500,) < 0) {
        klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 controller (command timed out)\n");
        return;
    }
    char mouse_id = inb(0x60);
    if (mouse_id == 0x00) {
        mouse_present = 1;
        int res = ps2_mouse_init();
        if (res < 0) {
            klog(LOG_ERROR, "ps2", "Failed to initialize PS/2 mouse\n");
        } else {
            klog(LOG_INFO, "ps2", "Successfully initialized PS/2 mouse\n");
        }
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
