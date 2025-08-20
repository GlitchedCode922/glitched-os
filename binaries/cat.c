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
    }

    uint64_t file_size = get_file_size(argv[1]);
    uint8_t buffer[file_size + 1];
    read_file(argv[1], buffer, 0, file_size);
    buffer[file_size] = '\0';
    printf("%s", buffer);
    return 0;
}

int _start(int argc, char** argv) {
    return main(argc, argv);
}
