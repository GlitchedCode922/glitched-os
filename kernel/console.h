#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "limine.h"

#define COLOR(r, g, b) ((uint8_t[]){(r), (g), (b)})

extern uint32_t width;
extern uint32_t height;

void initialize_console();
void putchar(char c);
void puts(const char *str);
void clear_screen();
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list args);
void kprintf_hex(uint64_t value);
void kprintf_dec(uint64_t value);
void kprintf_dec_signed(int64_t value);
void setbg_color(uint8_t color[3]);
void setfg_color(uint8_t color[3]);
void set_cursor_position(uint16_t x, uint16_t y);
void get_cursor_position(uint16_t *x, uint16_t *y);
void scroll();
