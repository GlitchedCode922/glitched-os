#include "uapi/spawn.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syscall.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

pid_t fork() {
    int res = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t spawn(const char* path, char *const *argv, char *const *envp, spawn_action_t** actions) {
    int res = syscall(SYSCALL_SPAWN, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, (uint64_t)actions, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

pid_t spawn_p(const char *file, char *const *argv, char *const *envp, spawn_action_t** actions) {
    if (!file || *file == '\0') {
        errno = ENOENT;
        return -1;
    }

    if (strchr(file, '/')) return spawn(file, argv, envp, actions);
    const char* path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    size_t file_len = strlen(file);
    const char *p = path;

    while (1) {
        const char* colon = strchr(p, ':');
        size_t dir_len = colon ? colon - p : strlen(p);
        char candidate[PATH_MAX];
        size_t len;
        if (dir_len == 0) {
            len = file_len + 2;
            if (len > PATH_MAX) {
                errno = ENAMETOOLONG;
                return -1;
            }
            candidate[0] = '.';
            candidate[1] = '/';
            memcpy(candidate + 2, file, file_len + 1);
        } else {
            int slash = p[dir_len - 1] == '/';
            len = dir_len + (slash ? 0 : 1) + file_len + 1;
            if (len > PATH_MAX) {
                errno = ENAMETOOLONG;
                return -1;
            }
            memcpy(candidate, p, dir_len);
            size_t off = dir_len;
            if (!slash) candidate[off++] = '/';
            memcpy(candidate + off, file, file_len + 1);
        }
        int res = spawn(candidate, argv, envp, actions);
        if (res > 0) return res;
        if (errno == ENOENT || errno == ENOTDIR) {
            if (!colon) break;
            p = colon + 1;
            continue;
        } else {
            return -1;
        }
    }
    return -1;
}

int execv(const char* path, char* const argv[]) {
    return execve(path, argv, environ);
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    int res = syscall(SYSCALL_EXECVE, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int execvp(const char *path, char *const *argv) {
    return execvpe(path, argv, environ);
}

int execvpe(const char *file, char *const *argv, char *const *envp) {
    if (!file || *file == '\0') {
        errno = ENOENT;
        return -1;
    }

    if (strchr(file, '/')) return execve(file, argv, envp);
    const char* path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    size_t file_len = strlen(file);
    const char *p = path;

    while (1) {
        const char* colon = strchr(p, ':');
        size_t dir_len = colon ? colon - p : strlen(p);
        char candidate[PATH_MAX];
        size_t len;
        if (dir_len == 0) {
            len = file_len + 2;
            if (len > PATH_MAX) {
                errno = ENAMETOOLONG;
                return -1;
            }
            candidate[0] = '.';
            candidate[1] = '/';
            memcpy(candidate + 2, file, file_len + 1);
        } else {
            int slash = p[dir_len - 1] == '/';
            len = dir_len + (slash ? 0 : 1) + file_len + 1;
            if (len > PATH_MAX) {
                errno = ENAMETOOLONG;
                return -1;
            }
            memcpy(candidate, p, dir_len);
            size_t off = dir_len;
            if (!slash) candidate[off++] = '/';
            memcpy(candidate + off, file, file_len + 1);
        }
        execve(candidate, argv, envp);
        if (errno == ENOENT || errno == ENOTDIR) {
            if (!colon) break;
            p = colon + 1;
            continue;
        } else {
            return -1;
        }
    }
    return -1;
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
