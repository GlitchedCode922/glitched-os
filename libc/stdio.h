#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define FLAG_CREATE 0x01
#define FLAG_NONBLOCKING 0x02

void scanf(const char* format, ...);
void sscanf(const char* str, const char* format, ...);
void printf(const char* format, ...);
void vprintf(const char* format, va_list args);
void puts(const char* str);
void putchar(char c);
char* readline(char* buffer, size_t size);

int list_directory(const char *path, char *element, uint64_t element_index);
int file_exists(const char* path);
int is_directory(const char *path);
uint64_t get_file_size(const char* path);
void remove_file(const char* path);
void create_file(const char* path);
void create_directory(const char* path);
int chdir(char* path);
void getcwd(char* buffer, size_t size);
