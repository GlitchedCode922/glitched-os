#include <stdio.h>
#include <errno.h>

int fileno(FILE* stream) {
    if (stream == NULL) {
        errno = EINVAL;
        return -1; // Error: stream is NULL
    }
    if (stream->handle < 0) {
        errno = EBADF;
        return -1; // Error: invalid file descriptor
    }
    return stream->handle;
}
