#include <stdio.h>

int main(int argc, char** argv) {
    char* path;
    if (argc < 2) {
        path = ".";
    } else {
        path = argv[1];
    }

    char buffer[13] = {0};
    int i = 0;
    do {
        int ret = list_directory(path, buffer, i);
        if (ret < 0) {
            perror("list failed");
            return 1;
        }
        if (*buffer) printf("%s\n", buffer);
        i++;
    } while (*buffer);
    return 0;
}
