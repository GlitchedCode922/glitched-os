#include <stdio.h>
#include <sys/types.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    FILE* fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror(argv[1]);
        return 1;
    }
    ssize_t bytes_read, bytes_written;
    char buffer[8192];
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) != 0) {
        if (bytes_read < 0) {
            perror("Error reading from source file");
            return 1;
        }
        bytes_written = fwrite(buffer, 1, bytes_read, stdout);
        if (bytes_written < 0) {
            perror("Error writing to destination");
            return 1;
        } else if (bytes_written != bytes_read) {
            printf("Error writing to destination\n");
            return 1;
        }
    }
    fclose(fp);

    return 0;
}
