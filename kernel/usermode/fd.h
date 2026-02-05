#pragma once
#include <stddef.h>
#include <stdint.h>

#define MAX_FDS 256
#define FD_TYPE_FILE 1
#define FD_TYPE_CONSOLE 2
#define FD_TYPE_FRAMEBUFFER 3

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

#define FILE_CREATE 0x01

typedef struct {
    int type;
    void* path;
    size_t offset;
    int refcount;
} fd_entry_t;

extern fd_entry_t fd_table[MAX_FDS];
extern fd_entry_t* fd_ptr_table[MAX_FDS];

int read(int fd, void* buffer, size_t size);
int write(int fd, const void* buffer, size_t size);
int seek(int fd, int64_t offset, int type);
int open_file(const char* path, uint16_t flags);
int open_console();
int open_framebuffer();
int open_disk(uint8_t disk);
int open_partition(uint8_t disk, uint8_t partition);
int close(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);
