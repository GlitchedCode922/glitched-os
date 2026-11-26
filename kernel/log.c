#include "log.h"
#include "io/ports.h"
#include "util/string.h"
#include "console.h"
#include <stddef.h>
#include <stdint.h>

log_level_t minimum_level = LOG_DEBUG;
log_level_t minimum_debugcon_level = LOG_INFO;
log_level_t minimum_console_level = LOG_WARN;

char log_buffer[4096];
int log_head = 0;

const size_t level_field_len = 5;
const size_t subsystem_field_len = 12;

void klog(log_level_t level, const char* subsystem, const char* string) {
    if (level < minimum_level) return;
    char buffer[4096];
    size_t len = 0;
    buffer[len++] = '[';
    char* level_str = NULL;
    switch (level) {
    case LOG_DEBUG: level_str = "debug"; break;
    case LOG_INFO:  level_str = "info "; break;
    case LOG_WARN:  level_str = "warn "; break;
    case LOG_ERROR: level_str = "error"; break;
    case LOG_FATAL: level_str = "fatal"; break;
    }
    memcpy(buffer + len, level_str, level_field_len);
    len += level_field_len;
    buffer[len++] = ']';

    buffer[len++] = ' ';

    buffer[len++] = '[';
    size_t subsystem_len = strlen(subsystem);
    memcpy(buffer + len, subsystem, subsystem_len);
    if (subsystem_len < subsystem_field_len) {
        memset(buffer + len + subsystem_len, ' ', subsystem_field_len - subsystem_len);
        len += subsystem_field_len;
    } else {
        len += subsystem_len;
    }
    buffer[len++] = ']';

    buffer[len++] = ' ';
    size_t string_len = strlen(string);
    if (string_len > 4094 - len) string_len = 4094 - len;
    memcpy(buffer + len, string, string_len);
    len += string_len;
    buffer[len] = '\0';

    uint64_t rflags;
    asm volatile("pushfq; popq %0; cli" : "=r"(rflags));

    // Send message to sinks
    // 1. Ring buffer
    char* p = buffer;
    while (*p++) {
        log_buffer[log_head] = *p;
        log_head = (log_head + 1) % 4096;
    }

    // 2. QEMU debugcon
    if (level >= minimum_debugcon_level) {
        p = buffer;
        while (*p) outb(0xE9, *p++);
    }

    // 3. TTY
    if (level >= minimum_console_level) {
        puts(buffer);
    }

    if (rflags & (1ULL << 9)) asm volatile("sti" ::: "memory");
}

static int hex_to_string(char* buf, uint64_t value) {
    char buffer[16];
    int i = 16;

    if (value == 0) {
        buffer[--i] = '0';
    } else {
        while (value > 0) {
            buffer[--i] = "0123456789ABCDEF"[value & 0xF]; // get last 4 bits
            value >>= 4;
        }
    }

    memcpy(buf, buffer + i, 16 - i);
    return 16 - i;
}

static int dec_to_string(char* buf, uint64_t value) {
    char buffer[20];
    int i = 20;

    if (value == 0) {
        buffer[--i] = '0';
    } else {
        while (value > 0) {
            buffer[--i] = '0' + (value % 10);
            value /= 10;
        }
    }

    memcpy(buf, buffer + i, 20 - i);
    return 20 - i;
}

static int signed_dec_to_string(char* buf, int64_t value) {
    if (value < 0) {
        *buf = '-';
        return dec_to_string(buf + 1, -value) + 1;
    } else {
        return dec_to_string(buf, value);
    }
}

void kvlogf(log_level_t level, const char* subsystem, const char *format, va_list args) {
    if (level < minimum_level) return;
    char buffer[4096];
    int len = 0;
    while (*format && len < 4095) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* s = va_arg(args, const char*);
                size_t string_len = strlen(s);
                memcpy(buffer + len, s, string_len);
                len += string_len;
            } else if (*format == 'c') {
                char c = (char)va_arg(args, int);
                buffer[len++] = c;
            } else if (*format == 'u') {
                uint64_t i = va_arg(args, uint64_t);
                len += dec_to_string(buffer + len, i);
            } else if (*format == 'd') {
                int64_t i = va_arg(args, int);
                len += signed_dec_to_string(buffer + len, i);
            } else if (*format == 'l') {
                int64_t i = va_arg(args, int64_t);
                len += signed_dec_to_string(buffer + len, i);
            } else if (*format == 'x') {
                uint64_t x = va_arg(args, uint64_t);
                len += hex_to_string(buffer + len, x);
            } else if (*format == 'p') {
                void *p = va_arg(args, void *);
                len += hex_to_string(buffer + len, (uintptr_t)p);
            } else if (*format == '%') {
                buffer[len++] = '%';
            }
        } else {
            buffer[len++] = *format;
        }
        format++;
    }
    buffer[len] = '\0';
    klog(level, subsystem, buffer);
}

void klogf(log_level_t level, const char *subsystem, const char *format, ...) {
    if (level < minimum_level) return;
    va_list args;
    va_start(args, format);
    kvlogf(level, subsystem, format, args);
    va_end(args);
}
