#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int64_t file_size = get_file_size(argv[1]);
    if (file_size < 0) {
        perror("Failed to get file size");
        return 1;
    }

    uint8_t buffer[file_size];
    int file_fd = open_file(argv[1], 0);
    if (file_fd < 0) {
        perror("Failed to open file");
        return 1;
    }

    int res = read(file_fd, buffer, file_size);
    if (res < 0) {
        perror("Failed to read file");
        close(file_fd);
        return 1;
    }

    res = write(STDOUT_FILENO, buffer, file_size);
    if (res < 0) {
        perror("Failed to write to stdout");
        close(file_fd);
        return 1;
    }
    close(file_fd);
    return 0;
}
