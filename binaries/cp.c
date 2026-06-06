#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int64_t file_size = get_file_size(argv[1]);
    if (file_size < 0) {
        perror("Failed to get file size");
        return 1;
    }
    unsigned char buffer[file_size];
    int file_fd = open_file(argv[1], 0);
    if (file_fd < 0) {
        perror(argv[1]);
        return 1;
    }
    int ret = read(file_fd, buffer, file_size);
    if (ret < 0) {
        perror("Read error");
        return 1;
    }
    close(file_fd);
    file_fd = open_file(argv[2], FLAG_CREATE);
    ret = write(file_fd, buffer, file_size);
    if (ret < 0) {
        perror("Write error");
        return 1;
    }
    close(file_fd);

    return 0;
}
