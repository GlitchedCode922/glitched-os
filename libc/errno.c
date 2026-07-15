#include "errno.h"
#include "unistd.h"
#include "string.h"

int errno = 0;

char* strerror(int errnum) {
    switch (errnum) {
        case EINVAL: return "Invalid argument";
        case EAGAIN: return "Resource temporarily unavailable";
        case ENOENT: return "No such file or directory";
        case EEXIST: return "File exists";
        case EISDIR: return "Is a directory";
        case ENOTDIR: return "Not a directory";
        case ENOMEM: return "Out of memory";
        case ENOSYS: return "Function not implemented";
        case EIO: return "Input/output error";
        case ENODEV: return "No such device";
        case EBADF: return "Bad file descriptor";
        case ENOSPC: return "No space left on device";
        case EROFS: return "Read-only file system";
        case ENOEXEC: return "Exec format error";
        case EMFILE: return "Too many open files";
        case ESPIPE: return "Illegal seek";
        case ENOTTY: return "Not a typewriter";
        case EBUSY: return "Device or resource busy";
        case ENOTBLK: return "Not a block device";
        case ENAMETOOLONG: return "Filename too long";
        case ENOTEMPTY: return "Directory not empty";
        case EXDEV: return "Invalid cross-device link";
        default: return "Unknown error";
    }
}

void perror(const char* s) {
    if (s) {
        write(STDERR_FILENO, s, strlen(s));
        write(STDERR_FILENO, ": ", 2);
    }
    const char* err_str = strerror(errno);
    write(STDERR_FILENO, err_str, strlen(err_str));
    write(STDERR_FILENO, "\n", 1);
}
