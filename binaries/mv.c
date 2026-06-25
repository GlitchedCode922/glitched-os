#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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
        printf("%s: No such file or directory\n", argv[1]);
        return 1;
    }

    char dest_path[2048];

    if (stat(argv[2], &st_dest) == 0 && st_dest.type == DT_DIR) {
        // Separate the filename from the source path
        char* filename = strrchr(argv[1], '/');
        if (filename) {
            filename++; // Move past the '/'
        } else {
            filename = argv[1]; // No '/' found, use the whole source path
        }
        // Construct the destination path
        strncpy(dest_path, argv[2], sizeof(dest_path) - 1);
        dest_path[sizeof(dest_path) - 1] = '\0'; // Ensure null-termination
        strncat(dest_path, "/", sizeof(dest_path) - strlen(dest_path) - 1);
        strncat(dest_path, filename, sizeof(dest_path) - strlen(dest_path) - 1);
    } else {
        strncpy(dest_path, argv[2], sizeof(dest_path) - 1);
        dest_path[sizeof(dest_path) - 1] = '\0'; // Ensure null-termination
    }

    // Attempt to rename the file or directory
    if (rename_file(argv[1], dest_path) == 0) {
        return 0;
    } 

    if (st_source.type == DT_DIR) {
        printf("Recursive move of directories is not implemented, renaming failed\n");
        return 1;
    }

    char buffer[8192];
    int fd_read = open(argv[1], 0);
    if (fd_read < 0) {
        perror(argv[1]);
        return 1;
    }
    int fd_write = open(dest_path, FLAG_CREATE);
    if (fd_write < 0) {
        perror(dest_path);
        return 1;
    }
    size_t bytes_read, bytes_written;
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
    int res = remove_file(argv[1]);
    if (res < 0) {
        perror("Error deleting source file");
        return 1;
    }
    close(fd_read);
    close(fd_write);

    return 0;
}
