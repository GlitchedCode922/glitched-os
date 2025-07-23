#include "console.h"
#include "bitmap.h"
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

char ascii[128][12] = ascii_set;

volatile struct limine_framebuffer *framebuffer = NULL;
volatile char *fb_address = NULL;

uint32_t width = 0;
uint32_t height = 0;
uint8_t padding_lines = 0;
uint8_t padding_pixels = 0;

uint8_t bg_color[3] = {0, 0, 0};
uint8_t fg_color[3] = {255, 255, 255};

uint16_t cursor_x = 0;
uint16_t cursor_y = 0;

#undef x
#undef y
#undef c
#undef s
#undef i
#undef p

void scroll() {
    if (!framebuffer) return;
    for (uint32_t y = padding_lines; y < framebuffer->height - 12 - padding_lines; y++) {
        char *scanline;
        scanline = (char*)(fb_address + (y * framebuffer->width + padding_pixels) * framebuffer->bpp / 8);
        for (int i=0; i < framebuffer->width; i++) {
            uint32_t dest_index = (y * framebuffer->width + i) * framebuffer->bpp / 8;
            uint32_t src_index = ((y + 12) * framebuffer->width + i) * framebuffer->bpp / 8;
            fb_address[dest_index + framebuffer->red_mask_shift/8] = fb_address[src_index + framebuffer->red_mask_shift/8];
            fb_address[dest_index + framebuffer->green_mask_shift/8] = fb_address[src_index + framebuffer->green_mask_shift/8];
            fb_address[dest_index + framebuffer->blue_mask_shift/8] = fb_address[src_index + framebuffer->blue_mask_shift/8];
        }
    }
    for (uint32_t y = framebuffer->height - 12 - padding_lines; y < framebuffer->height; y++) {
        for (uint32_t x = 0; x < framebuffer->width; x++) {
            uint32_t pixel_index = (y * framebuffer->width + x) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bg_color[0];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bg_color[1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bg_color[2];
        }
    }
}

void newline() {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= height) {
        scroll();
        cursor_y = height - 1;
    }
}

void clear_screen() {
    if (!framebuffer) return;

    for (uint32_t y = 0; y < framebuffer->height; y++) {
        for (uint32_t x = 0; x < framebuffer->width; x++) {
            uint32_t pixel_index = (y * framebuffer->width + x) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bg_color[0];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bg_color[1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bg_color[2];
        }
    }

    cursor_x = 0;
    cursor_y = 0;
}

void initialize_console(volatile struct limine_framebuffer *fb) {
    framebuffer = fb;
    fb_address = (volatile char *)framebuffer->address;
    width = framebuffer->width / 8;
    height = framebuffer->height / 12;
    padding_lines = (framebuffer->height % 12) / 2;
    padding_pixels = (framebuffer->width % 8) / 2;

    clear_screen();
}

char* colorize_bitmap(uint8_t index) {
    if (index >= 128) {
        return NULL; // Invalid character index
    }
    char *bitmap = ascii[index];
    static char colored_bitmap[12 * 8 * 3]; // 12 rows, 8 columns, 3 colors (RGB)
    for (int x = 0; x < 12; x++) {
        char row = bitmap[x];
        for (int y = 0; y < 8; y++) {
            int pixel_index = (x * 8 + y) * 3;
            if (row & (1 << (7 - y))) {
                colored_bitmap[pixel_index] = fg_color[0];
                colored_bitmap[pixel_index + 1] = fg_color[1];
                colored_bitmap[pixel_index + 2] = fg_color[2];
            } else {
                colored_bitmap[pixel_index] = bg_color[0];
                colored_bitmap[pixel_index + 1] = bg_color[1];
                colored_bitmap[pixel_index + 2] = bg_color[2];
            }
        }
    }
    return colored_bitmap;
}

void putchar(const char c) {
    if (!framebuffer || c < 0 || c >= 128) return;

    if (c == '\n') {
        newline();
        return;
    } else if (c == '\0') {
        return;
    }

    char *bitmap = colorize_bitmap(c);
    for (int x = 0; x < 12; x++) {
        for (int y = 0; y < 8; y++) {
            int pixel_index = (padding_lines + cursor_y * 12 + x) * framebuffer->width * framebuffer->bpp / 8 +
                              (padding_pixels + cursor_x * 8 + y) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bitmap[(x * 8 + y) * 3];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bitmap[(x * 8 + y) * 3 + 1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bitmap[(x * 8 + y) * 3 + 2];
        }
    }
    cursor_x++;
    if (cursor_x >= width) {
        newline();
    }
}

void puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            newline();
            str++;
            continue;
        }
        putchar(*str++);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                const char *s = va_arg(args, const char *);
                puts(s);
            } else if (*fmt == 'c') {
                char c = (char)va_arg(args, int);
                putchar(c);
            } else if (*fmt == 'd') {
                int i = va_arg(args, int);
                kprintf_dec(i);
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
    va_end(args);
}

void kvprintf(const char *fmt, va_list args){
    char buffer[256];
    char *buf_ptr = buffer;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                const char *s = va_arg(args, const char *);
                while (*s) {
                    *buf_ptr++ = *s++;
                }
            } else if (*fmt == 'c') {
                char c = (char)va_arg(args, int);
                *buf_ptr++ = c;
            }
        } else {
            *buf_ptr++ = *fmt;
        }
        fmt++;
    }
    *buf_ptr = '\0';
    puts(buffer);
}

void kprintf_hex(uint64_t value) {
    char buffer[17];
    for (int i = 15; i >= 0; i--) {
        buffer[i] = "0123456789ABCDEF"[value & 0xF];
        value >>= 4;
    }
    buffer[16] = '\0';
    puts(buffer);
}

void kprintf_dec(uint64_t value) {
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

void setbg_color(uint8_t color[3]) {
    bg_color[0] = color[0];
    bg_color[1] = color[1];
    bg_color[2] = color[2];
    clear_screen();
}

void setfg_color(uint8_t color[3]) {
    fg_color[0] = color[0];
    fg_color[1] = color[1];
    fg_color[2] = color[2];
}

void set_cursor_position(uint16_t x, uint16_t y) {
    if (x < width && y < height) {
        cursor_x = x;
        cursor_y = y;
    }
}

void get_cursor_position(uint16_t *x, uint16_t *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

