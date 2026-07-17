#include "rtc.h"
#include "../io/ports.h"
#include <stdint.h>
#include "../console.h"

static uint8_t rtc_read(uint8_t reg) {
    outb(0x70, 0x80 | reg);
    return inb(0x71);
}

static void rtc_write(uint8_t reg, uint8_t val) {
    outb(0x70, 0x80 | reg);
    outb(0x71, val);
}

static void enable_nmi() {
    outb(0x70, 0x00);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0f) + ((val >> 4) * 10);
}

int is_leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

uint64_t unix_timestamp(int year, int month, int day, int hour, int minute, int second) {
    uint64_t days = 0;
    for (int y = 1970; y < year; y++) {
        days += is_leap(y) ? 366 : 365;
    }

    static const int mdays[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    for (int m = 1; m < month; m++) {
        days += mdays[m - 1];
        if (m == 2 && is_leap(year)) days++;
    }
    days += day - 1;
    return days * 86400ULL + hour * 3600ULL + minute * 60ULL + second;
}

uint64_t get_timestamp() {
    while(!(rtc_read(0x0a) & 0x80));
    while(rtc_read(0x0a) & 0x80);

    uint32_t second = rtc_read(0x00);
    uint32_t minute = rtc_read(0x02);
    uint32_t hour   = rtc_read(0x04);
    uint32_t day    = rtc_read(0x07);
    uint32_t month  = rtc_read(0x08);
    uint32_t year   = rtc_read(0x09);
    uint32_t status = rtc_read(0x0b);

    int pm = hour & 0x80;
    hour &= 0x7f;
    if (!(status & 0x04)) {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour   = bcd_to_bin(hour);
        day    = bcd_to_bin(day);
        month  = bcd_to_bin(month);
        year   = bcd_to_bin(year);
    }
    if (!(status & 0x02)) {
        hour %= 12;
        if (pm) hour += 12;
    }
    year += 2000;
    return unix_timestamp(year, month, day, hour, minute, second);
}
