#include "serial.h"
#include <stdint.h>
#include "../io/ports.h"
#include "../io/8259pic.h"
#include "timer.h"

const uint16_t serial_ports[] = {COM1_PORT, COM2_PORT, COM3_PORT, COM4_PORT};
const uint8_t interrupt_numbers[] = {4, 3, 4, 3};
uint8_t enabled[] = {0, 0, 0, 0};

uint8_t buffers[4][1024];
uint16_t buffer_lengths[4] = {0, 0, 0, 0};
uint16_t read_heads[4] = {0, 0, 0, 0};
uint16_t write_heads[4] = {0, 0, 0, 0};

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
}

void serial_interrupt_handler(uint8_t irq) {
    for (int i = 0; i < 4; i++) {
        if (enabled[i] && interrupt_numbers[i] == irq) {
            uint16_t port = serial_ports[i];
            while (inb(port + 5) & 1) { // While data is available
                uint8_t byte = inb(port);
                // Store the byte in the buffer
                buffers[i][write_heads[i]] = byte;
                write_heads[i] = (write_heads[i] + 1) % 1024;
                if (write_heads[i] == read_heads[i]) {
                    // Buffer overflow, move read head to make space
                    read_heads[i] = (read_heads[i] + 1) % 1024;
                }
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

int serial_getc(int port, uint8_t *data, int blocking) {
    if (port < 1 || port > 4 || !enabled[port - 1]) {
        *data = 0; // Invalid port or not enabled
        return -1;
    }
    int index = port - 1;
    if (read_heads[index] == write_heads[index]) {
        if (blocking) {
            while (read_heads[index] == write_heads[index]);
        } else {
            *data = 0; // No data available
            return 0;
        }
    }
    *data = buffers[index][read_heads[index]];
    read_heads[index] = (read_heads[index] + 1) % 1024;
    return 1;
}

int serial_read(int port, char *buffer, size_t size, int blocking) {
    uint8_t byte;
    int result;
    for (size_t i = 0; i < size; i++) {
        if ((result = serial_getc(port, &byte, blocking)) < 0) {
            return i;
        }
        if (result == 0) {
            // No more data available
            return i;
        }
        buffer[i] = byte;
    }
    return size;
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

int serial_write(int port, const char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (serial_putc(port, buffer[i]) < 0) {
            return i;
        }
    }
    return size;
}
