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

int remove_file(const char* path);
int create_file(const char* path);
int create_directory(const char* path);
int rename_file(const char* old_path, const char* new_path);
int chdir(char* path);
void getcwd(char* buffer, size_t size);
