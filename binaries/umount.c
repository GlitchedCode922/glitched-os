#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <mountpoint>\n", argv[0]);
        return 1;
    }

    const char *mountpoint = argv[1];
    int result = umount(mountpoint);
    if (result < 0) {
        perror("umount failed");
        return 1;
    }
    return 0;
}
