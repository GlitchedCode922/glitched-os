#include <stdio.h>
#include <stdint.h>
#include <syscall.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <mountpoint>\n", argv[0]);
        printf("       %s all\n", argv[0]);
        return 1;
    }
    
    const char *mountpoint = argv[1];

    if (strcmp(mountpoint, "all") == 0) {
        int result = syscall(SYSCALL_UNMOUNT_ALL, 0, 0, 0, 0, 0);
        return result;
    }
    int result = syscall(SYSCALL_UNMOUNT, (uint64_t)mountpoint, 0, 0, 0, 0);
    return result;
}

int _start(int argc, char *argv[]) {
    return main(argc, argv);
}
