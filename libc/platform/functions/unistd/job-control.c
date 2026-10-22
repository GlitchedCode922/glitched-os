#include <errno.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/types.h>
#include <stdint.h>
#include <uapi/ioctl.h>

pid_t getpid() {
    int64_t res = syscall(SYSCALL_GETPID, 0, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t getppid() {
    int64_t res = syscall(SYSCALL_GET_PPID, 0, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t getpgid(pid_t pid) {
    int64_t res = syscall(SYSCALL_GETPGID, pid, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t getpgrp() {
    return getpgid(0);
}

pid_t setpgid(pid_t pid, pid_t pgid) {
    int64_t res = syscall(SYSCALL_SETPGID, pid, pgid, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t setpgrp() {
    return setpgid(0, 0);
}

pid_t getsid(pid_t pid) {
    int64_t res = syscall(SYSCALL_GETSID, pid, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t setsid() {
    int64_t res = syscall(SYSCALL_SETSID, 0, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int tcgetpgrp(int fd) {
    int64_t res = syscall(SYSCALL_IOCTL, fd, TCGETPGRP, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int tcsetpgrp(int fd, pid_t pgid) {
    int64_t res = syscall(SYSCALL_IOCTL, fd, TCSETPGRP, pgid, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
