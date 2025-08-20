#include <stdio.h>
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
        if (!is_directory(path_copy)) {
            if (file_exists(path_copy)) {
                printf("Cannot create a directory inside a file\n");
            } else {
                printf("Recursive directory creation is not allowed\n");
            }
            return 2;
        }
    }
    
    create_directory(argv[1]);
    return 0;
}

int _start(int argc, char** argv) {
    return main(argc, argv);
}
