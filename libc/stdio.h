#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define FLAG_CREATE 0x01
#define FLAG_NONBLOCKING 0x02

enum {
    DT_UNKNOWN = 0,
    DT_FILE = 1,
    DT_DIR = 2,
};

typedef struct {
    char name[256];
    uint32_t type;
} __attribute__((packed)) dirent_t;

typedef struct {
    uint64_t size;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t btime;
    uint32_t type;
} __attribute__((packed)) stat_t;

void scanf(const char* format, ...);
void sscanf(const char* str, const char* format, ...);
void printf(const char* format, ...);
void perror(const char* message);
void vprintf(const char* format, va_list args);
void puts(const char* str);
void putchar(char c);
char* readline(char* buffer, size_t size);

int list_directory(const char *path, char *element, uint64_t element_index);
int stat(const char* path, stat_t* out);
int remove_file(const char* path);
int create_file(const char* path);
int create_directory(const char* path);
int rename_file(const char* old_path, const char* new_path);
int chdir(char* path);
void getcwd(char* buffer, size_t size);
