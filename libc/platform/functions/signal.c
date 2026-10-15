#include "uapi/syscalls.h"
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <syscall.h>
#include <errno.h>
#include <uapi/signals.h>

int kill(pid_t pid, int sig) {
    int64_t res = syscall(SYSCALL_KILL, pid, sig, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int raise(int sig) {
    int64_t res = syscall(SYSCALL_RAISE, sig, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int sigaction(int sig, const sigaction_t *new, sigaction_t *old) {
    int64_t res = syscall(SYSCALL_SIGACTION, sig, (uint64_t)new, (uint64_t)old, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
