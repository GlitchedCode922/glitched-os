#include <stdio.h>
#include <stdint.h>
#include <syscall.h>

int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    int i = 0;

    // Handle negative numbers
    if (str[0] == '-') {
        sign = -1;
        i++;
    }

    for (; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            break; // Stop at the first non-digit character
        }
        result = result * 10 + (str[i] - '0');
    }

    return sign * result;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <disk> <partition> <mountpoint> <filesystem_type> [flags]\n", argv[0]);
        return 1;
    }
    
    int disk = atoi(argv[1]);
    int partition = atoi(argv[2]);
    const char *mountpoint = argv[3];
    const char *filesystem_type = argv[4];
    int flags = 0;
    if (argc >= 6) {
        flags = atoi(argv[5]);
    }

    int result = syscall(SYSCALL_MOUNT, disk, partition, (uint64_t)mountpoint, (uint64_t)filesystem_type, flags);
    return result;
}

int _start(int argc, char *argv[]) {
    return main(argc, argv);
}