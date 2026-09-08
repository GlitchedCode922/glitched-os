#include "fd.h"
#include "scheduler.h"
#include "../vfs.h"
#include "../error.h"
#include <stdint.h>

file_description_t file_descriptions[MAX_FILES] = {0};

static int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int fd_open(const char *path, uint16_t flags) {
    if (flags & O_WRONLY && flags & O_RDWR) return -EINVAL;
    stat_t st = {0};
    int res = stat(path, &st);
    if (res < 0) {
        if (flags & O_CREAT) {
            res = create_file(path);
            if (res < 0) return res;
        } else {
            return res;
        }
    } else if (flags & O_EXCL) {
        return -EEXIST;
    }
    if (flags & O_DIRECTORY && st.type != DT_DIR) return -ENOTDIR;

    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].fd == NULL) {
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -EMFILE;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_descriptions[i].refcount == 0) {
            int res = open(path, &file_descriptions[i].file_handle);
            if (res < 0) return res;
            file_descriptions[i].type = st.type == DT_DIR ? FD_TYPE_DIR : FD_TYPE_FILE;
            file_descriptions[i].offset = 0;
            file_descriptions[i].flags = flags & ~O_CLOEXEC;
            file_descriptions[i].refcount = 1;
            current_task->fd_table[fd_index].fd = &file_descriptions[i];
            current_task->fd_table[fd_index].flags = flags & O_CLOEXEC;
            return fd_index;
        }
    }
    return -ENFILE;
}

int fd_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (--file_description->refcount == 0) {
        if (file_description->type == FD_TYPE_FILE || file_description->type == FD_TYPE_DIR) {
            int res = close(file_description->file_handle);
            if (res < 0) return res;
        } else if (file_description->type == FD_TYPE_SOCKET) {
            int res = socket_close(file_description->socket);
            if (res < 0) return res;
        }
        file_description->offset = 0;
    }
    current_task->fd_table[fd].fd = NULL;
    current_task->fd_table[fd].flags = 0;
    return 0;
}

int64_t seek(int fd, int64_t offset, int type) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (type == SEEK_START) {
        file_description->offset = offset;
    } else if (type == SEEK_CURRENT) {
        file_description->offset += offset;
    } else if (type == SEEK_END) {
        stat_t st;
        int res = stat_handle(file_description->file_handle, &st);
        if (res < 0) return res;
        file_description->offset = st.size + offset;
    } else {
        return -EINVAL;
    }
    if (file_description->offset < 0) {
        file_description->offset = 0;
    }
    return file_description->offset;
}

int64_t tell(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    return file_description->offset;
}

int64_t read(int fd, void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type == FD_TYPE_FILE && (file_description->flags & O_ACCESS) == O_WRONLY) return -EBADF;
    if (file_description->type == FD_TYPE_DIR) return -EISDIR;
    if (file_description->type == FD_TYPE_SOCKET) return -ENOSYS;
    int64_t bytes_read;
    while (1) {
        bytes_read = read_file(file_description->file_handle, buffer, file_description->offset, size);
        if (file_description->flags & O_NONBLOCK || bytes_read != -EAGAIN) {
            break;
        }
        yield_current();
    }
    if (bytes_read > 0) file_description->offset += bytes_read;
    return bytes_read;
}

int64_t write(int fd, const void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type == FD_TYPE_FILE && (file_description->flags & O_ACCESS) == O_RDONLY) return -EBADF;
    if (file_description->type == FD_TYPE_DIR) return -EISDIR;
    if (file_description->type == FD_TYPE_SOCKET) return -ENOSYS;
    if (file_description->flags & O_APPEND) seek(fd, 0, SEEK_END);
    int64_t bytes_written = write_file(file_description->file_handle, buffer, file_description->offset, size);
    if (bytes_written > 0) file_description->offset += bytes_written;
    return bytes_written;
}

int fd_readdir(int fd, dirent_t* dirent) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type != FD_TYPE_DIR) return -ENOTDIR;
    int res = readdir(file_description->file_handle, file_description->offset, dirent);
    if (res > 0) file_description->offset += res;
    return res;
}

int fd_ioctl(int fd, uint64_t request, uint64_t arg) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type == FD_TYPE_DIR) return -EISDIR;
    return ioctl(file_description->file_handle, request, arg);
}

int dup(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].fd == NULL) {
            current_task->fd_table[i] = current_task->fd_table[fd];
            current_task->fd_table[i].flags = 0;
            current_task->fd_table[fd].fd->refcount++;
            return i;
        }
    }
    return -EMFILE;
}

int dup2(int fd, int new_fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    if (new_fd < 0 || new_fd >= MAX_FDS) {
        return -EBADF;
    }
    if (fd == new_fd) {
        return new_fd;
    }
    fd_close(new_fd);
    current_task->fd_table[new_fd] = current_task->fd_table[fd];
    current_task->fd_table[new_fd].flags = 0;
    current_task->fd_table[fd].fd->refcount++;
    return new_fd;
}

int fd_socket(int domain, int type, int protocol) {
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].fd == NULL) {
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -EMFILE;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_descriptions[i].refcount == 0) {
            int res = socket(domain, type, protocol, &file_descriptions[i].socket);
            if (res < 0) return res;
            file_descriptions[i].type = FD_TYPE_SOCKET;
            file_descriptions[i].offset = 0;
            file_descriptions[i].flags = 0;
            file_descriptions[i].refcount = 1;
            current_task->fd_table[fd_index].fd = &file_descriptions[i];
            current_task->fd_table[fd_index].flags = 0;
            return fd_index;
        }
    }
    return -ENFILE;
}

int fd_bind(int fd, sockaddr_in_t *addr) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type != FD_TYPE_SOCKET) return -EINVAL;
    return bind(file_description->socket, addr);
}

int fd_unbind(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type != FD_TYPE_SOCKET) return -EINVAL;
    return unbind(file_description->socket);
}

int64_t fd_recvfrom(int fd, uint8_t *buffer, uint64_t len, int flags, sockaddr_in_t *addr) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type != FD_TYPE_SOCKET) return -EINVAL;
    int64_t bytes_read;
    while (1) {
        bytes_read = recvfrom(file_description->socket, buffer, len, flags, addr);
        if (file_description->flags & O_NONBLOCK || bytes_read != -EAGAIN) {
            break;
        }
        yield_current();
    }
    return bytes_read;
}

int64_t fd_sendto(int fd, const uint8_t *buffer, uint64_t len, int flags, const sockaddr_in_t *addr) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_table[fd].fd == NULL) {
        return -EBADF;
    }
    file_description_t* file_description = current_task->fd_table[fd].fd;
    if (file_description->type != FD_TYPE_SOCKET) return -EINVAL;
    return sendto(file_description->socket, buffer, len, flags, addr);
}

void release_process_fds() {
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (current_task->fd_table[fd].fd == NULL) continue;
        fd_close(fd);
    }
}
