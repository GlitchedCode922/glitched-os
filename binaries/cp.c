#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int fd_read = open_file(argv[1], 0);
    if (fd_read < 0) {
        perror(argv[1]);
        return 1;
    }
    int fd_write = open_file(argv[2], FLAG_CREATE);
    if (fd_write < 0) {
        perror(argv[2]);
        return 1;
    }
    size_t bytes_read, bytes_written;
    char buffer[8192];
    while ((bytes_read = read(fd_read, buffer, sizeof(buffer))) != 0) {
        if (bytes_read < 0) {
            perror("Error reading from source file");
            return 1;
        }
        bytes_written = write(fd_write, buffer, bytes_read);
        if (bytes_written < 0) {
            perror("Error writing to destination file");
            return 1;
        } else if (bytes_written != bytes_read) {
            printf("Error writing to destination file\n");
            return 1;
        }
    }
    close(fd_read);
    close(fd_write);

    return 0;
}
