#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../vfs.h"
#include "../net/socket.h"

#define MAX_FDS 256

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

#define O_CREAT 0x01
#define O_NONBLOCK 0x02
#define O_DIRECTORY 0x04

enum {
    FD_TYPE_FILE = 0,
    FD_TYPE_DIR = 1,
    FD_TYPE_SOCKET = 2,
};

typedef struct {
    file_handle_t file_handle;
    socket_t* socket;
    int type;
    size_t offset;
    int flags;
    int refcount;
} fd_entry_t;

int64_t read(int fd, void* buffer, size_t size);
int64_t write(int fd, const void* buffer, size_t size);
int fd_readdir(int fd, dirent_t* dirent);
int fd_ioctl(int fd, uint64_t request, uint64_t arg);
int64_t seek(int fd, int64_t offset, int type);
int64_t tell(int fd);
int fd_open(const char* path, uint16_t flags);
int fd_close(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);
void release_process_fds();
int fd_socket(int domain, int type, int protocol);
int fd_bind(int fd, sockaddr_in_t* addr);
int fd_unbind(int fd);
int64_t fd_recvfrom(int fd, uint8_t* buffer, uint64_t len, int flags, sockaddr_in_t* addr);
int64_t fd_sendto(int fd, const uint8_t* buffer, uint64_t len, int flags, const sockaddr_in_t* addr);
