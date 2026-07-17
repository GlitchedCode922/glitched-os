#include "../io/ports.h"
#include "timer.h"
#include <stdint.h>

#define PIT_FREQUENCY 1000

uint64_t pit_ticks = 0;
uint64_t time_base = 0;

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

uint64_t get_time() {
    return time_base + get_uptime_seconds();
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

int is_leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

uint64_t to_unix_timestamp(datetime_t dt) {
    uint64_t days = 0;
    for (int y = 1970; y < dt.year; y++) {
        days += is_leap(y) ? 366 : 365;
    }

    static const int mdays[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    for (int m = 1; m < dt.month; m++) {
        days += mdays[m - 1];
        if (m == 2 && is_leap(dt.year)) days++;
    }
    days += dt.day - 1;
    return days * 86400ULL + dt.hour * 3600ULL + dt.minute * 60ULL + dt.second;
}

datetime_t to_datetime(uint64_t timestamp) {
    datetime_t dt;
    uint64_t days = timestamp / 86400;
    uint64_t seconds = timestamp % 86400;
    dt.hour = seconds / 3600;
    seconds %= 3600;
    dt.minute = seconds / 60;
    dt.second = seconds % 60;

    int year = 1970;
    while (1) {
        int days_in_year = is_leap(year) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }
    dt.year = year;

    static const int mdays[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    int month = 0;
    while (1) {
        int days_in_month = mdays[month];
        if (month == 1 && is_leap(year)) days_in_month++;
        if (days < days_in_month) break;
        days -= days_in_month;
        month++;
    }
    dt.month = month + 1;
    dt.day = days + 1;
    return dt;
}
