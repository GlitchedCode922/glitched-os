#include "unistd.h"
#include "syscall.h"

void yield() {
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

void sleep(uint64_t ms) {
    syscall(SYSCALL_SLEEP, ms, 0, 0, 0, 0);
}
