#include <stddef.h>
#include <stdio.h>

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
    int ret = read_file(argv[1], buffer, 0, file_size);
    if (ret < 0) {
        printf("Read error: %d\n", ret);
        return 1;
    }

    ret = write_file(argv[2], buffer, 0, file_size);
    if (ret < 0) {
        printf("Write error: %d\n", ret);
        return 1;
    }

    return 0;
}

int _start(int argc, char** argv) {
    return main(argc, argv);
}
