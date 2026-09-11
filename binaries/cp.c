#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    FILE* fp_read = fopen(argv[1], "r");
    if (fp_read == NULL) {
        perror(argv[1]);
        return 1;
    }
    FILE* fp_write = fopen(argv[2], "w");
    if (fp_write == NULL) {
        perror(argv[2]);
        return 1;
    }
    size_t bytes_read, bytes_written;
    char* buffer = malloc(65536);
    while ((bytes_read = fread(buffer, 1, 65536, fp_read)) != 0) {
        if (bytes_read < 0) {
            perror("Error reading from source file");
            return 1;
        }
        bytes_written = fwrite(buffer, 1, bytes_read, fp_write);
        if (bytes_written < 0) {
            perror("Error writing to destination file");
            return 1;
        } else if (bytes_written != bytes_read) {
            printf("Error writing to destination file\n");
            return 1;
        }
    }
    fclose(fp_read);
    fclose(fp_write);

    return 0;
}
