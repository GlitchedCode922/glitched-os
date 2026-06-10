#include "fd.h"
#include "scheduler.h"
#include "../vfs.h"
#include "../limine.h"
#include "../drivers/ps2_keyboard.h"
#include "../memory/mman.h"
#include "../error.h"
#include "../drivers/serial.h"
#include <stdint.h>

extern volatile struct limine_framebuffer* framebuffer;

static int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int open_file(const char *path, uint16_t flags) {
    stat_t st;
    int res = stat(path, &st);
    if (res < 0) {
        if (flags & FLAG_CREATE) {
            res = create_file(path);
            if (res < 0) return res;
        } else {
            return res;
        }
    }
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].type == 0) {
            current_task->fd_table[i].type = FD_TYPE_FILE;
            void* p = kmalloc(strlen(path) + 1);
            memcpy(p, path, strlen(path) + 1);
            current_task->fd_table[i].path = p;
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

int open_console(uint16_t flags) {
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].type == 0) {
            current_task->fd_table[i].type = FD_TYPE_TTY;
            current_task->fd_table[i].path = &keyboard_tty;
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

int open_framebuffer(uint16_t flags) {
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].type == 0) {
            current_task->fd_table[i].type = FD_TYPE_FRAMEBUFFER;
            current_task->fd_table[i].path = NULL;
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

int open_serial(int port, uint16_t flags) {
    if (!serial_port_exists(port)) {
        return 0;
    }
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current_task->fd_table[i].type == 0) {
            current_task->fd_table[i].type = FD_TYPE_TTY;
            current_task->fd_table[i].path = serial_ttys + port - 1;
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

int close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (--fd_entry->refcount == 0) {
        fd_entry->type = 0;
        kfree(fd_entry->path);
        fd_entry->path = NULL;
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
    if (fd_entry->type != FD_TYPE_FILE) {
        return -ESPIPE;
    }
    if (type == SEEK_START) {
        fd_entry->offset = offset;
    } else if (type == SEEK_CURRENT) {
        fd_entry->offset += offset;
    } else if (type == SEEK_END) {
        stat_t st;
        int res = stat(fd_entry->path, &st);
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

int read(int fd, void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_FILE) {
        int bytes_read = read_file(fd_entry->path, buffer, fd_entry->offset, size);
        if (bytes_read > 0) fd_entry->offset += bytes_read;
        return bytes_read;
    } else if (fd_entry->type == FD_TYPE_TTY) {
        return tty_read(fd_entry->path, buffer, size, !(fd_entry->flags & FLAG_NONBLOCKING));
    } else if (fd_entry->type == FD_TYPE_FRAMEBUFFER) {
        uint8_t* read_ptr = framebuffer->address + fd_entry->offset;
        size_t to_copy = size < (framebuffer->pitch * framebuffer->height - fd_entry->offset) ? size : (framebuffer->pitch * framebuffer->height - fd_entry->offset);
        memcpy(buffer, read_ptr, to_copy);
        fd_entry->offset += to_copy;
        return to_copy;
    }
    return -ENOSYS;
}

int write(int fd, const void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    fd_entry_t* fd_entry = current_task->fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_FILE) {
        int bytes_written = write_file(fd_entry->path, buffer, fd_entry->offset, size);
        if (bytes_written > 0) fd_entry->offset += bytes_written;
        return bytes_written;
    } else if (fd_entry->type == FD_TYPE_TTY) {
        return tty_write(fd_entry->path, (char*)buffer, size);
    } else if (fd_entry->type == FD_TYPE_FRAMEBUFFER) {
        uint8_t* write_ptr = framebuffer->address + fd_entry->offset;
        size_t to_copy = size < (framebuffer->pitch * framebuffer->height - fd_entry->offset) ? size : (framebuffer->pitch * framebuffer->height - fd_entry->offset);
        memcpy(write_ptr, buffer, to_copy);
        fd_entry->offset += to_copy;
        return to_copy;
    }
    return -ENOSYS;
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
    close(new_fd);
    current_task->fd_ptr_table[new_fd] = current_task->fd_ptr_table[fd];
    current_task->fd_ptr_table[fd]->refcount++;
    return new_fd;
}

int isatty(int fd) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    return current_task->fd_ptr_table[fd]->type == FD_TYPE_TTY;
}

int tcgetattr(int fd, termios_t *termios) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -EBADF;
    }
    if (!isatty(fd)) return -ENOTTY;
    *termios = ((tty_t*)current_task->fd_ptr_table[fd]->path)->termios;
    return 0;
}

int tcsetattr(int fd, termios_t *termios) {
    if (fd < 0 || fd >= MAX_FDS || current_task->fd_ptr_table[fd] == NULL) {
        return -1;
    }
    if (!isatty(fd)) return -2;
    ((tty_t*)current_task->fd_ptr_table[fd]->path)->termios = *termios;
    return 0;
}
