#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char** argv) {
    if (argc > 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int fd;
    if (argc == 2) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror(argv[1]);
            return 1;
        }
    } else {
        fd = STDIN_FILENO;
    }

    ssize_t bytes_read, bytes_written;
    char buffer[8192];
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) != 0) {
        if (bytes_read < 0) {
            perror("Error reading from source file");
            return 1;
        }
        bytes_written = write(STDOUT_FILENO, buffer, bytes_read);
        if (bytes_written < 0) {
            perror("Error writing to destination");
            return 1;
        } else if (bytes_written != bytes_read) {
            printf("Error writing to destination\n");
            return 1;
        }
    }
    close(fd);

    return 0;
}
