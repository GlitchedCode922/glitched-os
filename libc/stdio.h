#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

void scanf(const char* format, ...);
void sscanf(const char* str, const char* format, ...);
void printf(const char* format, ...);
void perror(const char* message);
void vprintf(const char* format, va_list args);
void puts(const char* str);
void putchar(char c);
char* readline(char* buffer, size_t size);
