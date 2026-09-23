#pragma once
#include <stdint.h>
#include <stdarg.h>

extern int console_tty_id;

void console_init(const char* device);
void putchar(char c);
void puts(const char *str);
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list args);
void kprintf_hex(uint64_t value);
void kprintf_dec(uint64_t value);
void kprintf_dec_signed(int64_t value);
