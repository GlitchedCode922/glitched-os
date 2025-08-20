#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    if (!file_exists(argv[1])) {
        printf("No such file or directory");
        return 1;
    }

    remove_file(argv[1]);
    return 0;
}

int _start(int argc, char** argv) {
    return main(argc, argv);
}
