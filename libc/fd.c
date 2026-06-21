#include "syscall.h"
#include "errno.h"
#include "unistd.h"

int read(int path, void* buffer, size_t size) {
    int res = syscall(SYSCALL_READ, (uint64_t)path, (uint64_t)buffer, (uint64_t)size, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int write(int path, const void* buffer, size_t size) {
    int res = syscall(SYSCALL_WRITE, (uint64_t)path, (uint64_t)buffer, (uint64_t)size, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int open_file(const char* path, uint16_t flags) {
    int res = syscall(SYSCALL_OPEN_FILE, (uint64_t)path, (uint64_t)flags, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int open_framebuffer(uint16_t flags) {
    int res = syscall(SYSCALL_OPEN_FRAMEBUFFER, (uint64_t)flags, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int close(int fd) {
    int res = syscall(SYSCALL_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int seek(int fd, int64_t offset, int type) {
    int res = syscall(SYSCALL_SEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)type, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int dup(int fd) {
    int res = syscall(SYSCALL_DUP, (uint64_t)fd, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int dup2(int fd, int new_fd) {
    int res = syscall(SYSCALL_DUP2, (uint64_t)fd, (uint64_t)new_fd, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
