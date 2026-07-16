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
            close(dirfd);
            perror("readdir failed");
            return 1;
        } else if (ret == 0) {
            close(dirfd);
            return 0;
        }
        if (dirent.type == DT_DIR) printf("\x1b[94m%s\x1b[0m\n", dirent.name);
        else if (dirent.type == DT_DIR) printf("\x1b[94m%s\x1b[0m\n", dirent.name);
        else if (dirent.type == DT_BLOCK) printf("\x1b[33m%s\x1b[0m\n", dirent.name);
        else if (dirent.type == DT_CHAR) printf("\x1b[93m%s\x1b[0m\n", dirent.name);
        else printf("%s\n", dirent.name);
    }
}
