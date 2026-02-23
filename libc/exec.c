#include "unistd.h"
#include "syscall.h"
#include <stdint.h>

pid_t fork() {
    return syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
}

pid_t spawn(const char* path, const char** argv) {
    return syscall(SYSCALL_SPAWN, (uint64_t)path, (uint64_t)argv, 0, 0, 0);
}

int execv(const char* path, const char** argv) {
    return syscall(SYSCALL_EXECV, (uint64_t)path, (uint64_t)argv, 0, 0, 0);
}

pid_t waitpid(pid_t pid, int* wstatus, int options) {
    return syscall(SYSCALL_WAITPID, pid, (uint64_t)wstatus, options, 0, 0);
}

pid_t wait(int *wstatus) {
    return waitpid(-1, wstatus, 0);
}
