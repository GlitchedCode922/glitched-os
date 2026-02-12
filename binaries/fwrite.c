#include <stdio.h>
#include <string.h>

int offset = 0;

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
            printf("%s: No such directory\n", path_copy);
            return 2;
        }
    }

    if (is_directory(argv[1])) {
        printf("%s: Is a directory\n", argv[1]);
        return 1;
    }

    while (1) {
        char console_line[256];
        read(STDIN_FILENO, console_line, sizeof(console_line));
        if (strcmp(console_line, "\\exit\n") == 0) return 0;
        int file_fd = open_file(argv[1], FLAG_CREATE);
        write(file_fd, console_line, strlen(console_line));
        close(file_fd);
    }
}
