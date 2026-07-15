#include "fd.h"
#include "scheduler.h"
#include "../vfs.h"
#include "../limine.h"
#include "../error.h"
#include <stdint.h>

extern volatile struct limine_framebuffer* framebuffer;

static int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int fd_open(const char *path, uint16_t flags) {
    stat_t st = {0};
    int res = stat(path, &st);
    if (res < 0) {
        if (flags & O_CREAT) {
            res = create_file(path);
            if (res < 0) return res;
        } else {
            return res;
        }
    }
    if (flags & O_DIRECTORY && st.type != DT_DIR) return -ENOTDIR;
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].refcount == 0) {
            int res = open(path, &current_task->fd_table[i].file_handle);
            if (res < 0) return res;
            current_task->fd_table[i].type = st.type == DT_DIR ? FD_TYPE_DIR : FD_TYPE_FILE;
            current_task->fd_table[i].offset = 0;
            current_task->fd_table[i].flags = flags;
            current_task->fd_table[i].refcount = 1;
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -EMFILE;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_ptr_table[i] == NULL) {
            current_task->fd_ptr_table[i] = current_task->fd_table + fd_index;
            return i;
        }
    }
    return -EMFILE;
}

int fd_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (--fd_entry->refcount == 0) {
        int res = close(fd_entry->file_handle);
        if (res < 0) return res;
        fd_entry->offset = 0;
    }
    current_task->fd_ptr_table[fd] = NULL;
    return 0;
}

int seek(int fd, int64_t offset, int type) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (type == SEEK_START) {
        fd_entry->offset = offset;
    } else if (type == SEEK_CURRENT) {
        fd_entry->offset += offset;
    } else if (type == SEEK_END) {
        stat_t st;
        int res = stat_handle(fd_entry->file_handle, &st);
        if (res < 0) return res;
        fd_entry->offset = st.size + offset;
    } else {
        return -EINVAL;
    }
    if (fd_entry->offset < 0) {
        fd_entry->offset = 0;
    }
    return 0;
}

int tell(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    return fd_entry->offset;
}

int read(int fd, void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_DIR) return -EISDIR;
    int bytes_read;
    while (1) {
        bytes_read = read_file(fd_entry->file_handle, buffer, fd_entry->offset, size);
        if (fd_entry->flags & O_NONBLOCK || bytes_read != -EAGAIN) {
            break;
        }
        yield_current();
    }
    if (bytes_read > 0) fd_entry->offset += bytes_read;
    return bytes_read;
}

int write(int fd, const void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_DIR) return -EISDIR;
    int bytes_written = write_file(fd_entry->file_handle, buffer, fd_entry->offset, size);
    if (bytes_written > 0) fd_entry->offset += bytes_written;
    return bytes_written;
}

int fd_readdir(int fd, dirent_t* dirent) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (fd_entry->type != FD_TYPE_DIR) return -ENOTDIR;
    int res = readdir(fd_entry->file_handle, fd_entry->offset, dirent);
    if (res > 0) fd_entry->offset += res;
    return res;
}

int fd_ioctl(int fd, uint64_t request, uint64_t arg) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_DIR) return -EISDIR;
    return ioctl(fd_entry->file_handle, request, arg);
}

int dup(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_ptr_table[i] == NULL) {
            current_task->fd_ptr_table[i] = current_task->fd_ptr_table[fd];
            current_task->fd_ptr_table[fd]->refcount++;
            return i;
        }
    }
    return -EMFILE;
}

int dup2(int fd, int new_fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    if (new_fd < 0 || new_fd >= MAX_FDS) {
        return -EBADF;
    }
    if (fd == new_fd) {
        return new_fd;
    }
    fd_close(new_fd);
    current_task->fd_ptr_table[new_fd] = current_task->fd_ptr_table[fd];
    current_task->fd_ptr_table[fd]->refcount++;
    return new_fd;
}

void release_process_fds() {
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (current_task->fd_ptr_table[fd] == NULL) continue;
        fd_close(fd);
    }
}
