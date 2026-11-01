#include <unistd.h>
#include <syscall.h>
#include <stdint.h>
#include <errno.h>

void yield() {
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0, 0);
}

int64_t sleep(uint64_t ms) {
    int64_t res = syscall(SYSCALL_SLEEP, ms, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
