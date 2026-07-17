#include "rtc.h"
#include "timer.h"
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

uint64_t rtc_get_timestamp() {
    while(!(rtc_read(0x0a) & 0x80));
    while(rtc_read(0x0a) & 0x80);

    datetime_t dt = {
        .second = rtc_read(0x00),
        .minute = rtc_read(0x02),
        .hour   = rtc_read(0x04),
        .day    = rtc_read(0x07),
        .month  = rtc_read(0x08),
        .year   = rtc_read(0x09),
    };
    uint8_t status = rtc_read(0x0b);

    int pm = dt.hour & 0x80;
    dt.hour &= 0x7f;
    if (!(status & 0x04)) {
        dt.second = bcd_to_bin(dt.second);
        dt.minute = bcd_to_bin(dt.minute);
        dt.hour   = bcd_to_bin(dt.hour);
        dt.day    = bcd_to_bin(dt.day);
        dt.month  = bcd_to_bin(dt.month);
        dt.year   = bcd_to_bin(dt.year);
    }
    if (!(status & 0x02)) {
        dt.hour %= 12;
        if (pm) dt.hour += 12;
    }
    dt.year += 2000;
    return to_unix_timestamp(dt);
}
