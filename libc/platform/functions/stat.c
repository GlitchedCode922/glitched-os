#include <sys/stat.h>
#include <unistd.h>
#include <syscall.h>
#include <errno.h>

int stat(const char *path, stat_t* out) {
    int res = syscall(SYSCALL_STAT, (uint64_t)path, (uint64_t)out, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int fstat(int fd, stat_t* out) {
    int res = syscall(SYSCALL_FSTAT, (uint64_t)fd, (uint64_t)out, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
