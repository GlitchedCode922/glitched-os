#include "../io/ports.h"
#include "timer.h"
#include <stdint.h>

#define PIT_FREQUENCY 1000

uint64_t pit_ticks = 0;

void pit_tick() {
    pit_ticks++;
}

uint64_t pit_get_ticks() {
    return pit_ticks;
}

void pit_set_frequency(uint32_t frequency) {
    if (frequency == 0) return; // Avoid division by zero

    uint32_t divisor = 1193180 / frequency; // PIT clock is 1.19318 MHz
    outb(0x43, 0x36); // Command byte: binary, mode 3, LSB/MSB
    outb(0x40, divisor & 0xFF); // Send LSB
    outb(0x40, (divisor >> 8) & 0xFF); // Send MSB
}

uint64_t get_uptime_seconds() {
    return pit_ticks / PIT_FREQUENCY;
}

uint64_t get_uptime_milliseconds() {
    return (pit_ticks * 1000) / PIT_FREQUENCY;
}

void timer_init() {
    pit_set_frequency(PIT_FREQUENCY);
    pit_ticks = 0; // Reset ticks
}
