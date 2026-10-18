#include <stdint.h>
#include <syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

int open(const char* path, uint16_t flags, ...) {
    int res = syscall(SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int fcntl(int fd, int request, ...) {
    int arg;
    if (request == F_GETFD || request == F_GETFL) {
        arg = 0;
    } else {
        va_list va;
        va_start(va, request);
        arg = va_arg(va, int);
        va_end(va);
    }
    int res = syscall(SYSCALL_FCNTL, (uint64_t)fd, (uint64_t)request, (uint64_t)arg, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
