#include "fd.h"
#include "../mount.h"
#include "../console.h"
#include "../limine.h"
#include "../drivers/ps2_keyboard.h"
#include "../memory/mman.h"
#include "../net/udp.h"
#include "../drivers/serial.h"
#include <stdint.h>

fd_entry_t fd_table[MAX_FDS] = {0};
fd_entry_t* fd_ptr_table[MAX_FDS] = {0};

extern volatile struct limine_framebuffer* framebuffer;

static int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int open_file(const char *path, uint16_t flags) {
    if (flags & FLAG_CREATE) {
        create_file(path);
    }
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_table[i].type == 0) {
            fd_table[i].type = FD_TYPE_FILE;
            void* p = kmalloc(strlen(path) + 1);
            memcpy(p, path, strlen(path) + 1);
            fd_table[i].path = p;
            fd_table[i].offset = 0;
            fd_table[i].serial_port = 0;
            fd_table[i].flags = flags;
            fd_table[i].refcount = 1;
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_ptr_table[i] == NULL) {
            fd_ptr_table[i] = fd_table + fd_index;
            return i;
        }
    }
    return -1;
}

int open_console(uint16_t flags) {
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_table[i].type == 0) {
            fd_table[i].type = FD_TYPE_CONSOLE;
            fd_table[i].path = NULL;
            fd_table[i].offset = 0;
            fd_table[i].serial_port = 0;
            fd_table[i].flags = flags;
            fd_table[i].refcount = 1;
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_ptr_table[i] == NULL) {
            fd_ptr_table[i] = fd_table + fd_index;
            return i;
        }
    }
    return -1;
}

int open_framebuffer(uint16_t flags) {
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_table[i].type == 0) {
            fd_table[i].type = FD_TYPE_FRAMEBUFFER;
            fd_table[i].path = NULL;
            fd_table[i].offset = 0;
            fd_table[i].serial_port = 0;
            fd_table[i].flags = flags;
            fd_table[i].refcount = 1;
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_ptr_table[i] == NULL) {
            fd_ptr_table[i] = fd_table + fd_index;
            return i;
        }
    }
    return -1;
}

int open_serial(int port, uint16_t flags) {
    if (!serial_port_exists(port)) {
        return -1;
    }
    int fd_index = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_table[i].type == 0) {
            fd_table[i].type = FD_TYPE_SERIAL;
            fd_table[i].path = NULL;
            fd_table[i].offset = 0;
            fd_table[i].serial_port = port;
            fd_table[i].flags = flags;
            fd_table[i].refcount = 1;
            fd_index = i;
            break;
        }
    }
    if (fd_index == -1) {
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_ptr_table[i] == NULL) {
            fd_ptr_table[i] = fd_table + fd_index;
            return i;
        }
    }
    return -1;
}

int close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || fd_ptr_table[fd] == NULL) {
        return -1;
    }
    fd_entry_t* fd_entry = fd_ptr_table[fd];
    if (--fd_entry->refcount == 0) {
        fd_entry->type = 0;
        kfree(fd_entry->path);
        fd_entry->path = NULL;
        fd_entry->offset = 0;
    }
    fd_ptr_table[fd] = NULL;
    return 0;
}

int seek(int fd, int64_t offset, int type) {
    if (fd < 0 || fd >= MAX_FDS || fd_ptr_table[fd] == NULL) {
        return -1;
    }
    fd_entry_t* fd_entry = fd_ptr_table[fd];
    if (type == SEEK_START) {
        fd_entry->offset = offset;
    } else if (type == SEEK_CURRENT) {
        fd_entry->offset += offset;
    } else if (type == SEEK_END) {
        // For files, we need to get the file size
        if (fd_entry->type == FD_TYPE_FILE) {
            size_t file_size = get_file_size((const char*)fd_entry->path);
            fd_entry->offset = file_size + offset;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
    if (fd_entry->offset < 0) {
        fd_entry->offset = 0;
    }
    return 0;
}

int read(int fd, void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || fd_ptr_table[fd] == NULL) {
        return -1;
    }
    fd_entry_t* fd_entry = fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_FILE) {
        int bytes_read = read_file(fd_entry->path, buffer, fd_entry->offset, size);
        fd_entry->offset += bytes_read;
        return bytes_read;
    } else if (fd_entry->type == FD_TYPE_CONSOLE) {
        return get_input(buffer, size, !(fd_entry->flags & FLAG_NONBLOCKING));
    } else if (fd_entry->type == FD_TYPE_FRAMEBUFFER) {
        uint8_t* read_ptr = framebuffer->address + fd_entry->offset;
        size_t to_copy = size < (framebuffer->pitch * framebuffer->height - fd_entry->offset) ? size : (framebuffer->pitch * framebuffer->height - fd_entry->offset);
        memcpy(buffer, read_ptr, to_copy);
        fd_entry->offset += to_copy;
        return to_copy;
    } else if (fd_entry->type == FD_TYPE_SERIAL) {
        return serial_read(fd_entry->serial_port, (char*)buffer, size, !(fd_entry->flags & FLAG_NONBLOCKING));
    }
    return -1;
}

int write(int fd, const void *buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FDS || fd_ptr_table[fd] == NULL) {
        return -1;
    }
    fd_entry_t* fd_entry = fd_ptr_table[fd];
    if (fd_entry->type == FD_TYPE_FILE) {
        int bytes_written = write_file(fd_entry->path, buffer, fd_entry->offset, size);
        fd_entry->offset += bytes_written;
        return bytes_written;
    } else if (fd_entry->type == FD_TYPE_CONSOLE) {
        for (size_t i = 0; i < size; i++) {
            putchar(((char*)buffer)[i]);
        }
        return size;
    } else if (fd_entry->type == FD_TYPE_FRAMEBUFFER) {
        uint8_t* write_ptr = framebuffer->address + fd_entry->offset;
        size_t to_copy = size < (framebuffer->pitch * framebuffer->height - fd_entry->offset) ? size : (framebuffer->pitch * framebuffer->height - fd_entry->offset);
        memcpy(write_ptr, buffer, to_copy);
        fd_entry->offset += to_copy;
        return to_copy;
    } else if (fd_entry->type == FD_TYPE_SERIAL) {
        serial_write(fd_entry->serial_port, (const char*)buffer, size);
        return size;
    }
    return -1;
}

int dup(int fd) {
    if (fd < 0 || fd >= MAX_FDS || fd_ptr_table[fd] == NULL) {
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_ptr_table[i] == NULL) {
            fd_ptr_table[i] = fd_ptr_table[fd];
            fd_ptr_table[fd]->refcount++;
            return i;
        }
    }
    return -1;
}

int dup2(int fd, int new_fd) {
    if (fd < 0 || fd >= MAX_FDS || fd_ptr_table[fd] == NULL) {
        return -1;
    }
    if (new_fd < 0 || new_fd >= MAX_FDS) {
        return -1;
    }
    if (fd == new_fd) {
        return new_fd;
    }
    close(new_fd);
    fd_ptr_table[new_fd] = fd_ptr_table[fd];
    fd_ptr_table[fd]->refcount++;
    return new_fd;
}
