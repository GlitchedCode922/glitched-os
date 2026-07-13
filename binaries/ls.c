#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    char* path;
    if (argc < 2) {
        path = ".";
    } else {
        path = argv[1];
    }

    dirent_t dirent;
    int dirfd = open(path, O_DIRECTORY);
    if (dirfd < 0) {
        perror("open failed");
        return 1;
    }
    while (1) {
        int ret = readdir(dirfd, &dirent);
        if (ret < 0) {
            perror("readdir failed");
            return 1;
        } else if (ret == 0) {
            return 0;
        }
        printf("%s\n", dirent.name);
    }
}
