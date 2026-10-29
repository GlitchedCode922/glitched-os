#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    // Remove trailing slashes
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        while (len > 0 && argv[i][len - 1] == '/') {
            argv[i][len - 1] = '\0';
            len--;
        }
    }

    stat_t st_source;
    stat_t st_dest;
    if (stat(argv[1], &st_source) != 0) {
        printf("%s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    char link_path[4096];

    if (stat(argv[2], &st_dest) == 0 && st_dest.type == DT_DIR) {
        // Separate the filename from the source path
        char* filename = strrchr(argv[1], '/');
        if (filename) {
            filename++; // Move past the '/'
        } else {
            filename = argv[1]; // No '/' found, use the whole source path
        }
        // Construct the link path
        strncpy(link_path, argv[2], sizeof(link_path) - 1);
        link_path[sizeof(link_path) - 1] = '\0';
        strncat(link_path, "/", sizeof(link_path) - strlen(link_path) - 1);
        strncat(link_path, filename, sizeof(link_path) - strlen(link_path) - 1);
    } else {
        strncpy(link_path, argv[2], sizeof(link_path) - 1);
        link_path[sizeof(link_path) - 1] = '\0';
    }

    int res = link(argv[1], link_path);
    if (res == 0) return 0;
    if (res < 0) {
        perror("link");
        return 1;
    }

    return 0;
}
