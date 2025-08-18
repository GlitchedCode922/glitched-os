#include "exec.h"
#include "syscall.h"
#include <stdint.h>

int exec(const char *path, const char **argv, const char **envp) {
    return syscall(SYSCALL_EXECUTE, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, 0, 0);
}
