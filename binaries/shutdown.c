#include <syscall.h>

int main() {
    syscall(SYSCALL_SHUTDOWN, 0, 0, 0, 0, 0, 0);
    return 0;
}
