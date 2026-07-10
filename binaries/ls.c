#include <stdio.h>

int main(int argc, char** argv) {
    char* path;
    if (argc < 2) {
        path = ".";
    } else {
        path = argv[1];
    }

    dirent_t dirent;
    int i = 0;
    while (1) {
        int ret = readdir(path, i, &dirent);
        if (ret < 0) {
            perror("readdir failed");
            return 1;
        } else if (ret == 0) {
            return 0;
        }
        printf("%s\n", dirent.name);
        i += ret;
    }
    return 0;
}
