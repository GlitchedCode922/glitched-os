#include "unistd.h"
#include "syscall.h"
#include "errno.h"
#include <stdint.h>

void* sbrk(intptr_t increment) {
    void* res = (void*)syscall(SYSCALL_SBRK, increment, 0, 0, 0, 0, 0);
    if (res == (void*)-1) errno = ENOMEM;
    return res;
}
