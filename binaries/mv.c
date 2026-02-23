#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    if (!file_exists(argv[1])) {
        printf("No such file or directory\n");
        return 1;
    }

    if (is_directory(argv[1])) {
        printf("%s: is a directory\n", argv[1]);
        return 1;
    }

    if (is_directory(argv[2])) {
        printf("%s: is a directory\n", argv[2]);
        return 1;
    }

    size_t file_size = get_file_size(argv[1]);
    unsigned char buffer[file_size];
    int file_fd = open_file(argv[1], 0);
    int ret = read(file_fd, buffer, file_size);
    if (ret < 0) {
        printf("Read error: %d\n", ret);
        return 1;
    }

    close(file_fd);
    file_fd = open_file(argv[2], FLAG_CREATE);
    ret = write(file_fd, buffer, file_size);
    if (ret < 0) {
        printf("Write error: %d\n", ret);
        return 1;
    }
    close(file_fd);
    remove_file(argv[1]);
    return 0;
}
