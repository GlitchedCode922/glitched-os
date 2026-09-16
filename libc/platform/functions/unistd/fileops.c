#include "uapi/syscalls.h"
#include <stdint.h>
#include <unistd.h>
#include <syscall.h>
#include <errno.h>

int readdir(int fd, dirent_t *out) {
    int res = syscall(SYSCALL_READDIR, fd, (uint64_t)out, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int link(const char* file, const char* link) {
    int res = syscall(SYSCALL_LINK, (uint64_t)file, (uint64_t)link, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int unlink(const char* path) {
    int res = syscall(SYSCALL_DELETE_FILE, (uint64_t)path, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int mkdir(const char* path, ...) {
    int res = syscall(SYSCALL_CREATE_DIR, (uint64_t)path, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int truncate(const char* path, uint64_t new_len) {
    int res = syscall(SYSCALL_TRUNCATE, (uint64_t)path, new_len, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int ftruncate(int fd, uint64_t new_len) {
    int res = syscall(SYSCALL_TRUNCATE, fd, new_len, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

void getcwd(char *buffer, size_t size) {
    syscall(SYSCALL_GETCWD, (uint64_t)buffer, size, 0, 0, 0, 0);
}

int chdir(char *path) {
    int res = syscall(SYSCALL_CHDIR, (uint64_t)path, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
