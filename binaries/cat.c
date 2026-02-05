#include <stdio.h>
#include <stdint.h>

int main(int argc, char** argv) {
    if (argc != 2) {
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

    uint64_t file_size = get_file_size(argv[1]);
    uint8_t buffer[file_size];
    int file_fd = open_file(argv[1], 0);
    read(file_fd, buffer, file_size);
    write(STDOUT_FILENO, buffer, file_size);
    close(file_fd);
    return 0;
}

int _start(int argc, char** argv) {
    return main(argc, argv);
}
