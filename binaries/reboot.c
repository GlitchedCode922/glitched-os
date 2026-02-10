#include "../libc/syscall.h"

int main() {
    syscall(SYSCALL_REBOOT, 0, 0, 0, 0, 0);
}