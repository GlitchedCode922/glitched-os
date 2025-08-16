#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

char* read_console();
void scanf(const char* format, ...);
void sscanf(const char* str, const char* format, ...);
void printf(const char* format, ...);
void vprintf(const char* format, va_list args);
void puts(const char* str);
void putchar(char c);

int read_file(const char* path, void* buffer, int offset, size_t size);
int write_file(const char* path, const void* buffer, int offset, size_t size);
uint64_t get_file_size(const char* path);
void remove_file(const char* path);
void create_file(const char* path);
void create_directory(const char* path);
