#pragma once
#include <stddef.h>
#include <stdint.h>

#define MAX_FDS 256
#define FD_TYPE_FILE 1
#define FD_TYPE_CONSOLE 2
#define FD_TYPE_FRAMEBUFFER 3
#define FD_TYPE_SERIAL 4

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

#define FLAG_CREATE 0x01

#define FLAG_NONBLOCKING 0x02

typedef struct {
    int type;
    void* path;
    size_t offset;
    int serial_port; // Only used if type is FD_TYPE_SERIAL
    int flags;
    int refcount;
} fd_entry_t;

extern fd_entry_t fd_table[MAX_FDS];
extern fd_entry_t* fd_ptr_table[MAX_FDS];

int read(int fd, void* buffer, size_t size);
int write(int fd, const void* buffer, size_t size);
int seek(int fd, int64_t offset, int type);
int open_file(const char* path, uint16_t flags);
int open_console(uint16_t flags);
int open_framebuffer(uint16_t flags);
int open_serial(int port, uint16_t flags);
int close(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);
