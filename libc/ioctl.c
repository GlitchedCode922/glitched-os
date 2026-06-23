#include "ioctl.h"
#include "syscall.h"
#include "errno.h"
#include <stdint.h>
#include <stdarg.h>

int ioctl(int fd, uint64_t request, ...) {
    va_list args;
    va_start(args, request);

    uint64_t param = va_arg(args, uint64_t);
    int result = syscall(SYSCALL_IOCTL, request, param, 0, 0, 0, 0);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}
