#include "stdio.h"
#include "syscall.h"
#include <stdarg.h>
#include <stdint.h>

char* read_console() {
    return (char*)syscall(SYSCALL_READ_CONSOLE, 0, 0, 0, 0, 0);
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
    char* buffer = read_console(); // Read input from console
    va_list args;
    va_start(args, format);
    
    vararg_sscanf(buffer, format, args);
    
    va_end(args);
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    syscall(SYSCALL_VPRINTF, (uint64_t)format, (uint64_t)&args, 0, 0, 0);

    va_end(args);
}

void vprintf(const char* format, va_list args) {
    syscall(SYSCALL_VPRINTF, (uint64_t)format, (uint64_t)&args, 0, 0, 0);
}

void putchar(char c) {
    char str[2];
    str[0] = c;
    str[1] = '\0';
    syscall(SYSCALL_WRITE_CONSOLE, (uint64_t)str, 0, 0, 0, 0);
}
