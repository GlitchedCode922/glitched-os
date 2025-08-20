#include <stdio.h>

int main(int argc, char** argv) {
    char* path;
    if (argc < 2) {
        path = ".";
    } else {
        path = argv[1];
    }

    if (!is_directory(path)) {
        printf("%s: Not a valid directory\n", path);
        return 1;
    }

    char buffer[13] = {0};
    int i = 0;
    do {
        int ret = list_directory(path, buffer, i);
        if (*buffer) printf("%s\n", buffer);
        i++;
    } while (*buffer);
    return 0;
}

int _start(int argc, char** argv) {
    return main(argc, argv);
}
