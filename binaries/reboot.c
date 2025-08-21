#include "../libc/syscall.h"

int _start() {
    syscall(SYSCALL_REBOOT, 0, 0, 0, 0, 0);
    return 0;
}