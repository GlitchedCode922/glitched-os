#include "console.h"
#include "drivers/ps2_keyboard.h"
#include "memory/mman.h"
#include "bitmap.h"
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

char ascii[128][12] = ascii_set;

extern volatile struct limine_framebuffer* framebuffer;
volatile char* fb_address = NULL;
character_t* console_buffer = NULL;

uint32_t width = 0;
uint32_t height = 0;
uint8_t padding_lines = 0;
uint8_t padding_pixels = 0;

uint8_t bg_color[3] = {0, 0, 0};
uint8_t fg_color[3] = {0xAA, 0xAA, 0xAA};

uint16_t cursor_x = 0;
uint16_t cursor_y = 0;

ansi_parser_t ansi_parser = {.state = TEXT};

uint8_t ansi_palette[16][3] = {
    {0x00, 0x00, 0x00}, // black
    {0xAA, 0x00, 0x00}, // red
    {0x00, 0xAA, 0x00}, // green
    {0xAA, 0x55, 0x00}, // yellow
    {0x00, 0x00, 0xAA}, // blue
    {0xAA, 0x00, 0xAA}, // magenta
    {0x00, 0xAA, 0xAA}, // cyan
    {0xAA, 0xAA, 0xAA}, // light gray
    {0x55, 0x55, 0x55}, // dark gray
    {0xFF, 0x55, 0x55}, // bright red
    {0x55, 0xFF, 0x55}, // bright green
    {0xFF, 0xFF, 0x55}, // bright yellow
    {0x55, 0x55, 0xFF}, // bright blue
    {0xFF, 0x55, 0xFF}, // bright magenta
    {0x55, 0xFF, 0xFF}, // bright cyan
    {0xFF, 0xFF, 0xFF}  // white
};

#undef x
#undef y
#undef c
#undef s
#undef i
#undef p
#undef n

void scroll() {
    if (!framebuffer) return;
    input_disabled++;

    uint8_t bg[3] = {bg_color[0], bg_color[1], bg_color[2]};
    uint8_t fg[3] = {fg_color[0], fg_color[1], fg_color[2]};

    for (uint32_t y = 0; y < height - 1; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (memcmp(&console_buffer[y * width + x], &console_buffer[(y + 1) * width + x], sizeof(character_t)) == 0) continue;
            bg_color[0] = console_buffer[(y + 1) * width + x].bg[0];
            bg_color[1] = console_buffer[(y + 1) * width + x].bg[1];
            bg_color[2] = console_buffer[(y + 1) * width + x].bg[2];
            fg_color[0] = console_buffer[(y + 1) * width + x].fg[0];
            fg_color[1] = console_buffer[(y + 1) * width + x].fg[1];
            fg_color[2] = console_buffer[(y + 1) * width + x].fg[2];
            char *bitmap = colorize_bitmap(console_buffer[(y + 1) * width + x].c, 0);
            for (int inner_x = 0; inner_x < 12; inner_x++) {
                for (int inner_y = 0; inner_y < 8; inner_y++) {
                    int pixel_index = (padding_lines + y * 12 + inner_x) * framebuffer->width * framebuffer->bpp / 8 +
                                    (padding_pixels + x * 8 + inner_y) * framebuffer->bpp / 8;
                    fb_address[pixel_index + framebuffer->red_mask_shift/8] = bitmap[(inner_x * 8 + inner_y) * 3];
                    fb_address[pixel_index + framebuffer->green_mask_shift/8] = bitmap[(inner_x * 8 + inner_y) * 3 + 1];
                    fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bitmap[(inner_x * 8 + inner_y) * 3 + 2];
                }
            }
        }
    }
    for (uint32_t y = framebuffer->height - 12 - padding_lines; y < framebuffer->height; y++) {
        for (uint32_t x = 0; x < framebuffer->width; x++) {
            uint32_t pixel_index = (y * framebuffer->width + x) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = 0;
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = 0;
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = 0;
        }
    }
    for (uint32_t y = 0; y < height - 1; y++) {
        for (uint32_t x = 0; x < width; x++) console_buffer[y * width + x] = console_buffer[(y + 1) * width + x];
    }
    for (uint32_t x = 0; x < width; x++) {
        character_t* entry = &console_buffer[(height - 1) * width + x];
        entry->c = ' ';
        memset(entry->bg, 0, 3);
        memset(entry->fg, 0, 3);
    };
    memcpy(bg_color, bg, 3);
    memcpy(fg_color, fg, 3);
    input_disabled--;
}

void clear_screen() {
    if (!framebuffer) return;
    input_disabled++;
    for (uint32_t y = 0; y < framebuffer->height; y++) {
        for (uint32_t x = 0; x < framebuffer->width; x++) {
            uint32_t pixel_index = (y * framebuffer->width + x) * framebuffer->bpp / 8;
            fb_address[pixel_index + framebuffer->red_mask_shift/8] = bg_color[0];
            fb_address[pixel_index + framebuffer->green_mask_shift/8] = bg_color[1];
            fb_address[pixel_index + framebuffer->blue_mask_shift/8] = bg_color[2];
        }
    }

    for (uint32_t i = 0; i < width * height; i++) {
        console_buffer[i].c = ' ';
        memcpy(console_buffer[i].bg, bg_color, 3);
        memcpy(console_buffer[i].fg, fg_color, 3);
    };
    input_disabled--;
}

void initialize_console() {
    fb_address = (volatile char *)framebuffer->address;
    width = framebuffer->width / 8;
    height = framebuffer->height / 12;
    padding_lines = (framebuffer->height % 12) / 2;
    padding_pixels = (framebuffer->width % 8) / 2;

    console_buffer = kmalloc(width * height * sizeof(character_t));

    clear_screen();
    set_cursor_position(0, 0);
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

    char *bitmap = colorize_bitmap(console_buffer[cursor_y * width + cursor_x].c, 1);
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
    uint8_t bg[3] = {bg_color[0], bg_color[1], bg_color[2]};
    uint8_t fg[3] = {fg_color[0], fg_color[1], fg_color[2]};
    memset(bg_color, 0, 3);
    memset(fg_color, 0, 3);
    if (old_x != cursor_x || old_y != cursor_y) {
        // Redraw the old cursor position to erase it
        char *old_bitmap = colorize_bitmap(console_buffer[old_y * width + old_x].c, 0);
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
    memcpy(bg_color, bg, 3);
    memcpy(fg_color, fg, 3);
    draw_cursor();
}

void newline() {
    int old_x = cursor_x;
    int old_y = cursor_y;
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= height) {
        scroll();
        old_y = height - 2;
        cursor_y = height - 1;
    }
    update_cursor(old_x, old_y);
}

void putchar(const char c) {
    if (!framebuffer || c < 0 || c >= 128) return;

    if (c == '\n') {
        newline();
        return;
    } else if (c == '\b') {
        int old_x = cursor_x;
        int old_y = cursor_y;
        if (cursor_x == 0) {
            if (cursor_y > 0) {
                cursor_x = width - 1;
                cursor_y--;
            }
        } else {
            cursor_x--;
        }
        update_cursor(old_x, old_y);
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
    console_buffer[cursor_y * width + cursor_x].c = c;
    memcpy(console_buffer[cursor_y * width + cursor_x].bg, bg_color, 3);
    memcpy(console_buffer[cursor_y * width + cursor_x].fg, fg_color, 3);
    cursor_x++;
    if (cursor_x >= width) {
        cursor_x = width - 1;
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

size_t console_echo(tty_t *tty, const char *buffer, size_t len) {
    for (int i = 0; i < len; i++) {
        putchar(buffer[i]);
    }
    return len;
}

size_t console_write(tty_t* tty, const char* buffer, size_t len) {
    for (int i = 0; i < len; i++) {
        if (ansi_parser.state == TEXT) {
            if (buffer[i] == '\033') {
                ansi_parser.state = ESCAPE;
                continue;
            }
            putchar(buffer[i]);
            continue;
        } else if (ansi_parser.state == ESCAPE) {
            if (buffer[i] == '[') {
                ansi_parser.state = CSI;
                memset(ansi_parser.params, 0, sizeof(ansi_parser.params));
                ansi_parser.param_count = 0;
                ansi_parser.current_param = 0;
                ansi_parser.digit_added = 0;
                continue;
            }
            ansi_parser.state = TEXT;
            continue;
        } else if (ansi_parser.state == CSI) {
            if (buffer[i] >= '0' && buffer[i] <= '9') {
                ansi_parser.current_param *= 10;
                ansi_parser.current_param += (buffer[i] - '0');
                ansi_parser.digit_added = 1;
            } else if (buffer[i] == ';' && ansi_parser.param_count < 16) {
                ansi_parser.params[ansi_parser.param_count++] = ansi_parser.current_param;
                ansi_parser.current_param = 0;
                ansi_parser.digit_added = 0;
            } else {
                if (ansi_parser.digit_added && ansi_parser.param_count < 16) ansi_parser.params[ansi_parser.param_count++] = ansi_parser.current_param;
                // End of sequence
                switch (buffer[i]) {
                    case 'H':
                    case 'f':
                        set_cursor_position(
                            ansi_parser.params[0] ? ansi_parser.params[0] - 1 : 0,
                            ansi_parser.params[1] ? ansi_parser.params[1] - 1 : 0
                        );
                        break;
                    case 'J':
                        clear_screen();
                        draw_cursor();
                        break;
                    case 'A': {
                        int n = (ansi_parser.params[0] ? ansi_parser.params[0] : 1);
                        set_cursor_position(cursor_x, cursor_y > n ? cursor_y - n : 0);
                        break;
                    }
                    case 'B': {
                        int n = (ansi_parser.params[0] ? ansi_parser.params[0] : 1);
                        int new_y = cursor_y + n;
                        if (new_y >= height) {
                            int scroll_amount = new_y - (height - 1);
                            for (int i = 0; i < scroll_amount; i++) scroll();
                            new_y = height - 1;
                        }
                        set_cursor_position(cursor_x, new_y);
                        break;
                    }
                    case 'C': {
                        int n = (ansi_parser.params[0] ? ansi_parser.params[0] : 1);
                        int new_x = cursor_x + n;
                        if (new_x >= width) new_x = width - 1;
                        set_cursor_position(new_x, cursor_y);
                        break;
                    }
                    case 'D': {
                        int n = (ansi_parser.params[0] ? ansi_parser.params[0] : 1);
                        int new_x = cursor_x - n;
                        if (new_x < 0) new_x = 0;
                        set_cursor_position(new_x, cursor_y);
                        break;
                    }
                    case 'm':
                        if (ansi_parser.params[0] == 38 && ansi_parser.params[1] == 2 && ansi_parser.param_count >= 5) {
                            fg_color[0] = ansi_parser.params[2];
                            fg_color[1] = ansi_parser.params[3];
                            fg_color[2] = ansi_parser.params[4];
                            break;
                        }

                        if (ansi_parser.params[0] == 48 && ansi_parser.params[1] == 2 && ansi_parser.param_count >= 5) {
                            bg_color[0] = ansi_parser.params[2];
                            bg_color[1] = ansi_parser.params[3];
                            bg_color[2] = ansi_parser.params[4];
                            break;
                        }

                        if (ansi_parser.param_count == 0) {
                            memset(bg_color, 0x00, 3);
                            memset(fg_color, 0xAA, 3);
                            break;
                        }

                        for (int i = 0; i < ansi_parser.param_count; i++) {
                            int p = ansi_parser.params[i];
                            if (p == 0) {
                                memset(bg_color, 0x00, 3);
                                memset(fg_color, 0xAA, 3);
                            } else if (p >= 30 && p <= 37) memcpy(fg_color, &ansi_palette[p - 30], 3);
                            else if (p >= 40 && p <= 47) memcpy(bg_color, &ansi_palette[p - 40], 3);
                            else if (p >= 90 && p <= 97) memcpy(fg_color, &ansi_palette[p - 90 + 8], 3);
                            else if (p >= 100 && p <= 107) memcpy(bg_color, &ansi_palette[p - 100 + 8], 3);
                        }
                        break;
                    default:
                        // Invalid sequence
                        break;
                }
                ansi_parser.state = TEXT;
            }
        }
    }
    return len;
}
