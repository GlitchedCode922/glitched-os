#include <stdint.h>
#include "syscall.h"
#include "unistd.h"
#include <stddef.h>
#include "stdio.h"

char* readline(char* buffer, size_t size) {
    char* buf_start = buffer;
    if (size == 0) return NULL;
    for (size_t i = 0; i < size - 1; i++) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) {
            if (i == 0) return NULL;
            break;
        }
        *buffer++ = c;
        if (c == '\n') {
            break; // End of line
        }
    }
    *buffer = '\0'; // Null-terminate the string
    return buf_start;
}

void vararg_sscanf(const char* str, const char *format, va_list args) {
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 'd') {
                int* int_ptr = va_arg(args, int*);
                int value = 0;
                while (*str >= '0' && *str <= '9') {
                    value = value * 10 + (*str - '0');
                    str++;
                }
                *int_ptr = value;
            } else if (*format == 's') {
                char* str_ptr = va_arg(args, char*);
                while (*str && *str != ' ') {
                    *str_ptr++ = *str++;
                }
                *str_ptr = '\0'; // Null-terminate the string
            } else if (*format == 'c') {
                char* char_ptr = va_arg(args, char*);
                if (*str) {
                    *char_ptr = *str++;
                } else {
                    *char_ptr = '\0'; // Null-terminate if no character is available
                }
            } else if (*format == 'x') {
                unsigned int* hex_ptr = va_arg(args, unsigned int*);
                unsigned int value = 0;
                while ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F')) {
                    value *= 16;
                    if (*str >= '0' && *str <= '9') {
                        value += *str - '0';
                    } else if (*str >= 'a' && *str <= 'f') {
                        value += *str - 'a' + 10;
                    } else if (*str >= 'A' && *str <= 'F') {
                        value += *str - 'A' + 10;
                    }
                    str++;
                }
                *hex_ptr = value;
            }
        } else {
            str++; // Move to the next character in the input string
        }
        format++;
    }
}

void sscanf(const char* str, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vararg_sscanf(str, format, args);

    va_end(args);
}

void scanf(const char *format, ...) {
    char buffer[1024];
    int bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    va_list args;
    va_start(args, format);

    vararg_sscanf(buffer, format, args);

    va_end(args);
}

void putchar(char c) {
    write(STDOUT_FILENO, &c, 1);
}

void puts(const char *str) {
    while (*str) {
        putchar(*str++);
    }
}

void printf_hex(uint64_t value) {
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

void printf_dec(uint64_t value) {
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

void printf_dec_signed(int64_t value) {
    if (value < 0) {
        putchar('-');
        printf_dec(-value);
    } else {
        printf_dec(value);
    }
}

void vprintf(const char *fmt, va_list args){
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
                printf_dec(i);
            } else if (*fmt == 'd') {
                int64_t i = va_arg(args, int64_t);
                printf_dec_signed(i);
            } else if (*fmt == 'x') {
                uint64_t x = va_arg(args, uint64_t);
                printf_hex(x);
            } else if (*fmt == 'p') {
                void *p = va_arg(args, void *);
                printf_hex((uint64_t)p);
            } else if (*fmt == '%') {
                putchar('%');
            }
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
}

void printf(const char *fmt, ...){
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

uint64_t get_file_size(const char* path) {
    return syscall(SYSCALL_GET_FILE_SIZE, (uint64_t)path, 0, 0, 0, 0);
}

int list_directory(const char *path, char *element, uint64_t element_index) {
    return syscall(SYSCALL_LIST_DIR, (uint64_t)path, (uint64_t)element, element_index, 0, 0);
}

int file_exists(const char *path) {
    return syscall(SYSCALL_FILE_EXISTS, (uint64_t)path, 0, 0, 0, 0);
}

int is_directory(const char *path) {
    return syscall(SYSCALL_IS_DIRECTORY, (uint64_t)path, 0, 0, 0, 0);
}

void remove_file(const char* path) {
    syscall(SYSCALL_DELETE_FILE, (uint64_t)path, 0, 0, 0, 0);
}

void create_file(const char* path) {
    syscall(SYSCALL_CREATE_FILE, (uint64_t)path, 0, 0, 0, 0);
}

void create_directory(const char* path) {
    syscall(SYSCALL_CREATE_DIR, (uint64_t)path, 0, 0, 0, 0);
}

void getcwd(char *buffer, size_t size) {
    syscall(SYSCALL_GETCWD, (uint64_t)buffer, size, 0, 0, 0);
}

int chdir(char *path) {
    return syscall(SYSCALL_CHDIR, (uint64_t)path, 0, 0, 0, 0);
}
