#include "console.h"
#include "drivers/tty.h"
#include "fs/devfs.h"
#include "panic.h"
#include "uapi/dirent.h"
#include "uapi/stat.h"
#include "vfs.h"
#include <stdint.h>

int console_tty_id = 0; // Set to first registered TTY for early kernel panics

void console_init(const char *device) {
    uint64_t handle;
    int res = devfs_lookup(device, &handle);
    if (res < 0) panic("Couldn't find kernel console device: %s", device);
    stat_t st;
    res = devfs_stat(handle, &st);
    if (res < 0) panic("Couldn't stat kernel console device: %s", device);
    if (st.type != DT_CHAR) panic("Specified console device (%s) is not a TTY", device);
    if (major(st.rdev) != tty_driver_index) panic("Specified console device (%s) is not a TTY", device);
    console_tty_id = minor(st.rdev);
    ttys[console_tty_id]->termios = (termios_t){
        .c_iflag = ICRNL,
        .c_oflag = OPOST,
        .c_lflag = ICANON | ECHO | ECHOE | ECHOCTL,
    };
    devfs_mknod("console", DT_CHAR, makedev(tty_driver_index, console_tty_id));
}

void putchar(char c) {
    if (tty_count == 0) return;
    tty_write(console_tty_id, 0, (uint8_t*)&c, 1);
}

void puts(const char *str) {
    if (tty_count == 0) return;
    while (*str) putchar(*str++);
}

void kprintf(const char *fmt, ...) {
    if (tty_count == 0) return;
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

void kvprintf(const char *fmt, va_list args){
    if (tty_count == 0) return;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                const char *s = va_arg(args, const char *);
                puts(s);
            } else if (*fmt == 'c') {
                char c = (char)va_arg(args, int);
                putchar(c);
            } else if (*fmt == 'u') {
                uint64_t i = va_arg(args, uint64_t);
                kprintf_dec(i);
            } else if (*fmt == 'd') {
                int64_t i = va_arg(args, int);
                kprintf_dec_signed(i);
            } else if (*fmt == 'l') {
                int64_t i = va_arg(args, int64_t);
                kprintf_dec_signed(i);
            } else if (*fmt == 'x') {
                uint64_t x = va_arg(args, uint64_t);
                kprintf_hex(x);
            } else if (*fmt == 'p') {
                void *p = va_arg(args, void *);
                kprintf_hex((uint64_t)p);
            } else if (*fmt == '%') {
                putchar('%');
            }
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
}

void kprintf_hex(uint64_t value) {
    if (tty_count == 0) return;
    char buffer[17]; // 16 hex digits + null terminator
    int i = 16;
    buffer[i--] = '\0'; // null terminator at the end

    if (value == 0) {
        buffer[i--] = '0';
    } else {
        while (value > 0) {
            buffer[i--] = "0123456789ABCDEF"[value & 0xF]; // get last 4 bits
            value >>= 4;
        }
    }

    puts(&buffer[i + 1]);
}

void kprintf_dec(uint64_t value) {
    if (tty_count == 0) return;
    char buffer[21]; // 20 digits + null terminator
    int i = 20;
    buffer[i--] = '\0';
    if (value == 0) {
        buffer[i--] = '0';
    } else {
        while (value > 0) {
            buffer[i--] = '0' + (value % 10);
            value /= 10;
        }
    }
    puts(&buffer[i + 1]);
}

void kprintf_dec_signed(int64_t value) {
    if (tty_count == 0) return;
    if (value < 0) {
        putchar('-');
        kprintf_dec(-value);
    } else {
        kprintf_dec(value);
    }
}
