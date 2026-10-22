#include "uapi/dirent.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char** argv) {
    char* path;
    if (argc < 2) {
        path = ".";
    } else {
        path = argv[1];
    }

    DIR* dir = opendir(path);
    if (!dir) {
        perror("ls");
        return 1;
    }
    while (1) {
        errno = 0;
        dirent_t* dirent = readdir(dir);
        if (!dirent) {
            if (errno > 0) {
                perror("ls");
                return 1;
            } else {
                return 0;
            }
        }
        if (dirent->type == DT_DIR) printf("\x1b[94m%s\x1b[0m\n", dirent->name);
        else if (dirent->type == DT_DIR) printf("\x1b[94m%s\x1b[0m\n", dirent->name);
        else if (dirent->type == DT_BLOCK) printf("\x1b[33m%s\x1b[0m\n", dirent->name);
        else if (dirent->type == DT_CHAR) printf("\x1b[93m%s\x1b[0m\n", dirent->name);
        else printf("%s\n", dirent->name);
    }
    if (closedir(dir) < 0) {
        perror("closedir");
        return -1;
    }
    return 0;
}
