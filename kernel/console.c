#include "console.h"
#include "memory/mman.h"
#include "bitmap.h"
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

char ascii[128][12] = ascii_set;

extern volatile struct limine_framebuffer* framebuffer;
volatile char* fb_address = NULL;
volatile char* console_buffer = NULL;

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

void clear_screen() {
    if (!framebuffer) return;
    asm volatile("cli");
    for (uint32_t y = 0; y < framebuffer->height; y++) {
        for (uint32_t x = 0; x < framebuffer->width; x++) {
            uint32_t pixel_index = (y * framebuffer->width + x) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bg_color[0];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bg_color[1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bg_color[2];
            console_buffer[y * width + x] = ' ';
        }
    }

    cursor_x = 0;
    cursor_y = 0;
    asm volatile("sti");
}

void initialize_console() {
    fb_address = (volatile char *)framebuffer->address;
    width = framebuffer->width / 8;
    height = framebuffer->height / 12;
    padding_lines = (framebuffer->height % 12) / 2;
    padding_pixels = (framebuffer->width % 8) / 2;

    console_buffer = (volatile char*)kmalloc(framebuffer->width * framebuffer->height * framebuffer->bpp / 8);

    clear_screen();
}

char* colorize_bitmap(uint8_t index, int inv) {
    if (index >= 128) {
        return NULL; // Invalid character index
    }
    char *bitmap = ascii[index];
    static char colored_bitmap[12 * 8 * 3]; // 12 rows, 8 columns, 3 colors (RGB)
    char fg[3];
    char bg[3];
    for (int i = 0; i < 3; i++) {
        fg[i] = inv ? bg_color[i] : fg_color[i];
        bg[i] = inv ? fg_color[i] : bg_color[i];
    }
    for (int x = 0; x < 12; x++) {
        char row = bitmap[x];
        for (int y = 0; y < 8; y++) {
            int pixel_index = (x * 8 + y) * 3;
            if (row & (1 << (7 - y))) {
                colored_bitmap[pixel_index] = fg[0];
                colored_bitmap[pixel_index + 1] = fg[1];
                colored_bitmap[pixel_index + 2] = fg[2];
            } else {
                colored_bitmap[pixel_index] = bg[0];
                colored_bitmap[pixel_index + 1] = bg[1];
                colored_bitmap[pixel_index + 2] = bg[2];
            }
        }
    }
    return colored_bitmap;
}

void draw_cursor() {
    if (!framebuffer) return;

    char *bitmap = colorize_bitmap(console_buffer[cursor_y * width + cursor_x], 1);
    for (int x = 0; x < 12; x++) {
        for (int y = 0; y < 8; y++) {
            int pixel_index = (padding_lines + cursor_y * 12 + x) * framebuffer->width * framebuffer->bpp / 8 +
                              (padding_pixels + cursor_x * 8 + y) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bitmap[(x * 8 + y) * 3];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bitmap[(x * 8 + y) * 3 + 1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bitmap[(x * 8 + y) * 3 + 2];
        }
    }
}

void update_cursor(int old_x, int old_y) {
    if (old_x != cursor_x || old_y != cursor_y) {
        // Redraw the old cursor position to erase it
        char *old_bitmap = colorize_bitmap(console_buffer[old_y * width + old_x], 0);
        for (int x = 0; x < 12; x++) {
            for (int y = 0; y < 8; y++) {
                int pixel_index = (padding_lines + old_y * 12 + x) * framebuffer->width * framebuffer->bpp / 8 +
                                  (padding_pixels + old_x * 8 + y) * framebuffer->bpp / 8;
                fb_address[pixel_index + framebuffer->red_mask_shift/8] = old_bitmap[(x * 8 + y) * 3];
                fb_address[pixel_index + framebuffer->green_mask_shift/8] = old_bitmap[(x * 8 + y) * 3 + 1];
                fb_address[pixel_index + framebuffer->blue_mask_shift/8] = old_bitmap[(x * 8 + y) * 3 + 2];
            }
        }
    }
    draw_cursor();
}

void newline() {
    asm volatile("cli");
    int old_x = cursor_x;
    int old_y = cursor_y;
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= height) {
        scroll();
        cursor_y = height - 1;
    }
    update_cursor(old_x, old_y);
    asm volatile("sti");
}

void putchar(const char c) {
    if (!framebuffer || c < 0 || c >= 128) return;

    if (c == '\n') {
        newline();
        return;
    } else if (c == '\0') {
        return;
    }

    char *bitmap = colorize_bitmap(c, 0);
    for (int x = 0; x < 12; x++) {
        for (int y = 0; y < 8; y++) {
            int pixel_index = (padding_lines + cursor_y * 12 + x) * framebuffer->width * framebuffer->bpp / 8 +
                              (padding_pixels + cursor_x * 8 + y) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bitmap[(x * 8 + y) * 3];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bitmap[(x * 8 + y) * 3 + 1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bitmap[(x * 8 + y) * 3 + 2];
        }
    }
    console_buffer[cursor_y * width + cursor_x] = c;
    cursor_x++;
    if (cursor_x >= width) {
        newline();
        return;
    }
    draw_cursor();
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
                int64_t i = va_arg(args, int64_t);
                kprintf_dec_signed(i);
            } else if (*fmt == 'u') {
                uint64_t i = va_arg(args, uint64_t);
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
    if (value < 0) {
        putchar('-');
        kprintf_dec(-value);
    } else {
        kprintf_dec(value);
    }
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
        int old_x = cursor_x;
        int old_y = cursor_y;
        cursor_x = x;
        cursor_y = y;
        update_cursor(old_x, old_y);
    }
}

void get_cursor_position(uint16_t *x, uint16_t *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

