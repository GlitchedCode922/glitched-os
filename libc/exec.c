#include "unistd.h"
#include "errno.h"
#include "syscall.h"
#include <stdint.h>

pid_t fork() {
    int res = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t spawn(const char* path, const char** argv) {
    int res = syscall(SYSCALL_SPAWN, (uint64_t)path, (uint64_t)argv, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int execv(const char* path, const char** argv) {
    int res = syscall(SYSCALL_EXECV, (uint64_t)path, (uint64_t)argv, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t waitpid(pid_t pid, int* wstatus, int options) {
    int res = syscall(SYSCALL_WAITPID, pid, (uint64_t)wstatus, options, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t wait(int *wstatus) {
    int res = waitpid(-1, wstatus, 0);
    if (res < 0) {
        return -1;
    }
    return res;
}
