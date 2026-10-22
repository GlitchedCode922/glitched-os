#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <syscall.h>
#include <errno.h>
#include <stdio.h>

DIR* opendir(const char* path) {
    int fd = open(path, O_DIRECTORY);
    if (fd < 0) return NULL;
    DIR* ret = malloc(sizeof(DIR));
    if (!ret) return NULL;
    ret->fd = fd;
    ret->output = malloc(sizeof(dirent_t));
    if (!ret->output) return NULL;
    return ret;
}

DIR* fdopendir(int fd) {
    DIR* ret = malloc(sizeof(DIR));
    if (!ret) return NULL;
    ret->fd = fd;
    ret->output = malloc(sizeof(dirent_t));
    if (!ret->output) return NULL;
    return ret;
}

dirent_t* readdir(DIR* stream) {
    if (!stream) {
        errno = -EFAULT;
        return NULL;
    }
    int res = syscall(SYSCALL_READDIR, stream->fd, (uint64_t)stream->output, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return NULL;
    } else if (res == 0) {
        return NULL;
    }
    return stream->output;
}

off_t telldir(DIR* stream) {
    if (!stream) {
        errno = -EFAULT;
        return -1;
    }
    return tell(stream->fd);
}

void seekdir(DIR* stream, off_t loc) {
    if (!stream) {
        errno = -EFAULT;
        return;
    }
    lseek(stream->fd, loc, SEEK_SET);
}

int closedir(DIR* stream) {
    if (!stream) {
        errno = -EFAULT;
        return -1;
    }
    free(stream->output);
    int res = close(stream->fd);
    free(stream);
    return res;
}
