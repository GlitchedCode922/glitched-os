#include "ports.h"
#include "8259pic.h"
#include "../drivers/timer.h"
#include "../drivers/ps2_keyboard.h"

int spurious_interrupts = 0;

void pic_remap(int offset1, int offset2) {
    unsigned char a1, a2;

    // Save masks
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    // Start initialization in cascade mode
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    // Set vector offsets
    outb(PIC1_DATA, offset1);  // Master PIC vector offset
    outb(PIC2_DATA, offset2);  // Slave PIC vector offset

    // Tell Master PIC that Slave PIC is at IRQ2 (0000 0100)
    outb(PIC1_DATA, 4);

    // Tell Slave PIC its cascade identity (IRQ2)
    outb(PIC2_DATA, 2);

    // Set 8086/88 mode
    outb(PIC1_DATA, 1);
    outb(PIC2_DATA, 1);

    // Restore saved masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_enable_irq(int irq) {
    unsigned char mask;

    if (irq < 8) {
        // Enable IRQ on Master PIC
        mask = inb(PIC1_DATA) & ~(1 << irq);
        outb(PIC1_DATA, mask);
    } else {
        // Enable IRQ on Slave PIC
        mask = inb(PIC2_DATA) & ~(1 << (irq - 8));
        outb(PIC2_DATA, mask);
    }
}

void pic_disable_irq(int irq) {
    unsigned char mask;

    if (irq < 8) {
        // Disable IRQ on Master PIC
        mask = inb(PIC1_DATA) | (1 << irq);
        outb(PIC1_DATA, mask);
    } else {
        // Disable IRQ on Slave PIC
        mask = inb(PIC2_DATA) | (1 << (irq - 8));
        outb(PIC2_DATA, mask);
    }
}

void pic_send_eoi(int irq) {
    if (irq >= 8) {
        // Send EOI to Slave PIC
        outb(PIC2_CMD, 0x20);
    }
    // Send EOI to Master PIC
    outb(PIC1_CMD, 0x20);
}

void pic_init() {
    // Remap the PICs to avoid conflicts with other hardware
    pic_remap(32, 40);

    // Enable all IRQs
    outb(PIC1_DATA, 0x00); // Master PIC
    outb(PIC2_DATA, 0x00); // Slave PIC
}

void irq0_handler() {
    // Handle IRQ0 (timer interrupt)
    
    pit_tick();

    // Acknowledge the interrupt
    pic_send_eoi(0); // Send EOI to PIC for IRQ0
}

void irq1_handler() {
    // Handle IRQ1 (keyboard interrupt)
    
    uint8_t scancode = inb(0x60); // Read scancode from keyboard data port

    ps2_interrupt_handler(scancode);

    pic_send_eoi(1); // Send EOI to PIC for IRQ1
}

void irq7_handler() {
    // Check for spurious interrupts
    outb(PIC1_CMD, 0x0b); // Read ISR register
    uint8_t isr = inb(PIC1_CMD);
    if(isr & (1 << 7)) {
        spurious_interrupts++;
    }
}

void irq15_handler() {
    outb(PIC2_CMD, 0x0b); // Read ISR register for Slave PIC
    uint8_t isr = inb(PIC2_CMD);
    if(isr & (1 << 7)) {
        spurious_interrupts++;
        pic_send_eoi(7);
    }
}

void irq_handler(int irq) {
    switch (irq) {
        case 0:
            irq0_handler();
            break;
        case 1:
            irq1_handler();
            break;
        case 7:
            irq7_handler();
            break;
        case 15:
            irq15_handler();
            break;
        default:
            pic_send_eoi(irq); // Acknowledge the interrupt
            break;
    }
}
