#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

int64_t getline(char** lineptr, size_t* n, FILE* stream) {
    if (lineptr == NULL || n == NULL || stream == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (*lineptr == NULL || *n == 0) {
        char* buffer = malloc(128);
        if (buffer == NULL) return -1;

        *lineptr = buffer;
        *n = 128;
    }

    size_t length = 0;

    while (1) {
        int c = fgetc(stream);

        if (c == EOF) {
            if (ferror(stream) || length == 0) return -1;
            break;
        }

        if (length >= (size_t)((int64_t)-1 > 0 ? (int64_t)-1 : ((size_t)-1 >> 1))) {
            errno = EOVERFLOW;
            return -1;
        }

        if (length + 1 >= *n) {
            if (*n > (size_t)-1 / 2) {
                errno = EOVERFLOW;
                return -1;
            }

            size_t capacity = *n * 2;
            char* buffer = realloc(*lineptr, capacity);
            if (buffer == NULL) return -1;

            *lineptr = buffer;
            *n = capacity;
        }

        (*lineptr)[length++] = (char)c;
        if (c == '\n') break;
    }

    (*lineptr)[length] = '\0';
    return (int64_t)length;
}
