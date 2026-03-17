#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "limine.h"
#include "drivers/tty.h"

#define COLOR(r, g, b) ((uint8_t[]){(r), (g), (b)})

extern uint32_t width;
extern uint32_t height;

typedef struct {
    enum {
        TEXT,
        ESCAPE,
        CSI
    } state;
    int params[16];
    int current_param;
    int param_count;
    int digit_added;
} ansi_parser_t;

typedef struct {
    char c;
    uint8_t fg[3];
    uint8_t bg[3];
    uint8_t padding;
} character_t;

typedef struct {
    char* ascii[128];
    uint8_t width;
    uint8_t height;
} font_t;

void initialize_console();
void setfont(font_t* font);
char* colorize_bitmap(uint8_t index, int inv);
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
size_t console_echo(tty_t* tty, const char* buffer, size_t len);
size_t console_write(tty_t* tty, const char* buffer, size_t len);
