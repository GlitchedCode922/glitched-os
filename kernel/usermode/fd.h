#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../drivers/tty.h"

#define MAX_FDS 256

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

#define FLAG_CREATE 0x01
#define FLAG_NONBLOCKING 0x02

typedef struct {
    void* path;
    size_t offset;
    int flags;
    int refcount;
} fd_entry_t;

int read(int fd, void* buffer, size_t size);
int write(int fd, const void* buffer, size_t size);
int fd_ioctl(int fd, uint64_t request, uint64_t arg);
int seek(int fd, int64_t offset, int type);
int tell(int fd);
int open(const char* path, uint16_t flags);
int close(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);
