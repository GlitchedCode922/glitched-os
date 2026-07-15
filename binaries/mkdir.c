#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }
    // Remove the last segment to check if parent exists
    int last_slash = -1;
    char path_copy[strlen(argv[1]) + 1];
    strcpy(path_copy, argv[1]);
    int i = 0;

    while (path_copy[i]) {
        if (path_copy[i] == '/') {
            last_slash = i;
        }
        i++;
    }

    if (last_slash != -1) {
        path_copy[last_slash + 1] = '\0';
        stat_t st;
        int res = stat(path_copy, &st);
        if (res < 0) {
            printf("Recursive directory creation is not allowed\n");
            return 1;
        } else if (st.type != DT_DIR) {
            printf("Cannot create a directory inside a file\n");
            return 1;
        }
    }

    int res = create_directory(argv[1]);
    if (res < 0) {
        perror("Failed to create directory");
        return 1;
    }
    return 0;
}
