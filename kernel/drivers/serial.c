#include "serial.h"
#include <stddef.h>
#include <stdint.h>
#include "../io/ports.h"
#include "../io/8259pic.h"
#include "timer.h"
#include "tty.h"

const uint16_t serial_ports[] = {COM1_PORT, COM2_PORT, COM3_PORT, COM4_PORT};
const uint8_t interrupt_numbers[] = {4, 3, 4, 3};
uint8_t enabled[] = {0, 0, 0, 0};

tty_t serial_ttys[4] = {{0}};

void serial_init() {
    for (int i = 0; i < 4; i++) {
        uint16_t port = serial_ports[i];
        outb(port + 1, 0x00); // Disable all interrupts
        outb(port + 3, 0x80); // Enable DLAB (set baud rate divisor)
        outb(port + 0, 0x02); // Set divisor to 2 (lo byte) 57600 baud
        outb(port + 1, 0x00); //                  (hi byte)
        outb(port + 3, 0x03); // 8 bits, no parity, one stop bit
        outb(port + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
        // Loopback test
        outb(port + 4, 0x10);
        outb(port + 0, 0xAE); // Test serial chip (send byte 0xAE and check if it's received)

        const uint64_t start_tick = pit_get_ticks(); // e.g., PIT, APIC, or RTC tick
        const uint64_t timeout_ticks = 50; // number of ticks to wait

        while (!(inb(port + 5) & 0x01)) {
            if ((pit_get_ticks() - start_tick) > timeout_ticks) {
                goto next;
            }
        }
        uint8_t received = inb(port + 0);
        if (received != 0xAE) {
            // Serial port not working, skip enabling it
            continue;
        }

        outb(port + 4, 0x0B); // IRQs enabled, RTS/DSR set
        outb(port + 1, 0x01); // Enable received data available interrupt
        pic_enable_irq(interrupt_numbers[i]);
        enabled[i] = 1;
next:
        continue;
    }
    serial_ttys[0].port = 1;
    serial_ttys[1].port = 2;
    serial_ttys[2].port = 3;
    serial_ttys[3].port = 4;
    serial_ttys[0].echo = serial_write;
    serial_ttys[1].echo = serial_write;
    serial_ttys[2].echo = serial_write;
    serial_ttys[3].echo = serial_write;
    serial_ttys[0].write = serial_write;
    serial_ttys[1].write = serial_write;
    serial_ttys[2].write = serial_write;
    serial_ttys[3].write = serial_write;
}

void serial_interrupt_handler(uint8_t irq) {
    for (int i = 0; i < 4; i++) {
        if (enabled[i] && interrupt_numbers[i] == irq) {
            uint16_t port = serial_ports[i];
            while (inb(port + 5) & 1) { // While data is available
                uint8_t byte = inb(port);
                tty_char_recv(serial_ttys + i, byte);
            }
            break;
        }
    }
}

int serial_port_exists(int port) {
    if (port < 1 || port > 4 || !enabled[port - 1]) {
        return 0;
    }
    return 1;
}

int serial_putc(int port, uint8_t data) {
    if (port < 1 || port > 4 || !enabled[port - 1]) {
        return -1; // Invalid port or not enabled
    }
    uint16_t port_addr = serial_ports[port - 1];
    while (!(inb(port_addr + 5) & 0x20)); // Wait for the transmitter holding register to be empty
    outb(port_addr, data);
    return 1;
}

size_t serial_write(tty_t* tty, const char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (serial_putc(tty->port, buffer[i]) < 0) {
            return i;
        }
    }
    return size;
}
