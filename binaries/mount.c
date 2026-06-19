#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

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
    if (argc < 4) {
        printf("Usage: %s <source> <target> <filesystem_type> [flags]\n", argv[0]);
        return 1;
    }

    const char* source = argv[1];
    const char* target = argv[2];
    const char* filesystem_type = argv[3];
    int flags = 0;
    if (argc >= 5) {
        flags = atoi(argv[4]);
    }

    int result = mount(source, target, filesystem_type, flags);
    if (result < 0) {
        perror("Mount failed");
        return 1;
    }
    return result;
}
