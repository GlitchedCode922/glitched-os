#include "stdlib.h"
#include "syscall.h"
#include <stdint.h>

void exit(int status) {
    syscall(SYSCALL_EXIT, (uint64_t)status, 0, 0, 0, 0, 0);
    while (1); // Prevent warning
}
