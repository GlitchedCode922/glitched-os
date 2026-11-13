#include "io/ports.h"
#include "io/8259pic.h"
#include "io/pci.h"
#include "io/ps2.h"
#include "usermode/scheduler.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include <stdint.h>

irq_handler_t irq_handlers[16] = {0};

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

    // Disable all IRQs
    outb(PIC1_DATA, 0xFF); // Master PIC
    outb(PIC2_DATA, 0xFF); // Slave PIC
    pic_enable_irq(2);
}

void irq0_handler(iframe_t* iframe) {
    // Handle IRQ0 (timer interrupt)

    pit_tick();

    // Acknowledge the interrupt
    pic_send_eoi(0); // Send EOI to PIC for IRQ0

    check_blocked_tasks(1);
    ticks_remaining--;
    if (ticks_remaining <= 0 && (iframe->cs == USER_CS || current_task->is_kworker)) {
        run_next(iframe); // Next task
    }
}

void irq_handler(int irq, iframe_t* iframe) {
    pci_irq_handler(irq);
    if (irq_handlers[irq].handler) {
        irq_handlers[irq].handler(irq_handlers[irq].arg);
    }
    if (irq == 0) {
        irq0_handler(iframe);
        return;
    }
    pic_send_eoi(irq);
}
