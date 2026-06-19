#include "unistd.h"
#include "errno.h"
#include "syscall.h"
#include <stdint.h>

int mount(const char* source, const char* target, const char* type, int flags) {
    int result = syscall(SYSCALL_MOUNT, (uint64_t)source, (uint64_t)target, (uint64_t)type, flags, 0, 0);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}

int umount(const char* mountpoint) {
    int result = syscall(SYSCALL_UNMOUNT, (uint64_t)mountpoint, 0, 0, 0, 0, 0);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}

int umount_all() {
    int result = syscall(SYSCALL_UNMOUNT_ALL, 0, 0, 0, 0, 0, 0);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return 0;
}
