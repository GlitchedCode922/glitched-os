#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int file_fd = open_file(argv[1], FLAG_CREATE);
    if (file_fd < 0) {
        perror(argv[1]);
        return 1;
    }
    char console_line[256];
    while (1) {
        readline(console_line, sizeof(console_line));
        if (strcmp(console_line, "\\exit\n") == 0) return 0;
        int res = write(file_fd, console_line, strlen(console_line));
        if (res < 0) {
            perror("Write error");
            return 1;
        }
    }
    close(file_fd);
}
