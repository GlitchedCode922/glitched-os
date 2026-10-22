#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <syscall.h>
#include <errno.h>
#include <uapi/signals.h>
#include <uapi/syscalls.h>

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

int signal(int sig, void (*handler)(int)) {
    sigaction_t sa = {.handler = handler};
    return sigaction(sig, &sa, NULL);
}

int sigaction(int sig, const sigaction_t *new, sigaction_t *old) {
    int64_t res = syscall(SYSCALL_SIGACTION, sig, (uint64_t)new, (uint64_t)old, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int sigemptyset(sigset_t* set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

int sigfillset(sigset_t* set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = UINT32_MAX;
    return 0;
}

int sigaddset(sigset_t* set, int sig) {
    if (!set || sig < 1 || sig > 31) {
        errno = EINVAL;
        return -1;
    }
    *set |= (1 << (sig - 1));
    return 0;
}

int sigdelset(sigset_t* set, int sig) {
    if (!set || sig < 1 || sig > 31) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1 << (sig - 1));
    return 0;
}

int sigismember(sigset_t* set, int sig) {
    if (!set || sig < 1 || sig > 31) {
        errno = EINVAL;
        return -1;
    }
    return (*set & (1 << (sig - 1))) != 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* old_set) {
    int64_t res = syscall(SYSCALL_SIGPROCMASK, how, (uint64_t)set, (uint64_t)old_set, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

char* strsignal(int sig) {
    switch (sig) {
        case SIGABRT: return "Aborted";
        case SIGFPE: return "Floating point exception";
        case SIGILL: return "Illegal instruction";
        case SIGINT: return "Keyboard interrupt";
        case SIGSEGV: return "Segmentation fault";
        case SIGTERM: return "Terminated";
        case SIGKILL: return "Killed";
        default: return "Unknown signal";
    }
}
