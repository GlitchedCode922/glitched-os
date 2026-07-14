#include "ramfs.h"
#include "../memory/mman.h"
#include "../drivers/block.h"
#include "../drivers/chrdev.h"
#include "../error.h"
#include <stdint.h>

static ramfs_mount_t* mount;

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int ramfs_get_dirent(const char* path, ramfs_dirent_t** out) {
    while (*path == '/') path++;
    ramfs_dirent_t* current = &mount->root;
    while (*path) {
        char subdir[256];
        int count = 0;
        while (*path != '/' && *path && count < 256) {
            subdir[count++] = *path++;
        }
        if (count == 0) {
            if (*path == '/') path++;  // Consume slash
            continue;
        }
        subdir[count] = '\0';
        if (current->type != DT_DIR) return -ENOENT;
        current = current->child;
        int found = 0;
        while (current) {
            if (strcmp(subdir, current->name) == 0) {
                // Found
                found = 1;
                break;
            }
            current = current->next;
        }
        if (!found) return -ENOENT;
        count = 0;
    }
    *out = current;
    return 0;
}

int ramfs_free_dirent(ramfs_dirent_t* dirent) {
    if (dirent->previous) {
        dirent->previous->next = dirent->next;
    }
    if (dirent->parent->child == dirent) {
        dirent->parent->child = dirent->next;
    }
    if (dirent->next) {
        dirent->next->previous = dirent->previous;
    }
    int data_pointer_count = dirent->file_size / RAMFS_BLOCK_SIZE + 1;
    ramfs_data_t* data_pointers[data_pointer_count];
    memset(data_pointers, 0, sizeof(data_pointers));
    data_pointers[0] = dirent->first_block;
    for (int i = 1; i < data_pointer_count; i++) {
        if (data_pointers[i - 1] == NULL) break;
        data_pointers[i] = data_pointers[i - 1]->next;
    }
    for (int i = data_pointer_count - 1; i >= 0; i--) {
        kfree(data_pointers[i]);
    }
    kfree(dirent);
    return 0;
}

int ramfs_lookup(const char* path, uint64_t* handle) {
    ramfs_dirent_t* dirent;
    int res = ramfs_get_dirent(path, &dirent);
    if (res < 0) return res;
    *handle = (uint64_t)dirent;
    return 0;
}

int ramfs_open(uint64_t handle) {
    ramfs_dirent_t* dirent = (ramfs_dirent_t*)handle;
    dirent->open_count++;
    return 0;
}

int ramfs_close(uint64_t handle) {
    ramfs_dirent_t* dirent = (ramfs_dirent_t*)handle;
    dirent->open_count--;
    if (dirent->open_count == 0 && dirent->to_delete) {
        return ramfs_free_dirent(dirent);
    }
    return 0;
}

int ramfs_readdir(uint64_t handle, int index, dirent_t *out) {
    ramfs_dirent_t* dirent = (ramfs_dirent_t*)handle;
    if (dirent->type != DT_DIR) return -ENOTDIR;
    dirent = dirent->child;
    int i = 0;
    while (dirent) {
        if (dirent->to_delete) {
            dirent = dirent->next;
            continue;
        }
        if (i == index) {
            memcpy(out->name, dirent->name, 256);
            out->type = dirent->type;
            return 1;
        }
        dirent = dirent->next;
        i++;
    }
    return 0;
}

int ramfs_read(uint64_t handle, uint8_t *buffer, size_t offset, size_t size) {
    ramfs_dirent_t* dirent = (ramfs_dirent_t*)handle;
    if (dirent->type == DT_DIR) return -EISDIR;
    if (dirent->type == DT_BLOCK) {
        block_device_t dev = {
            .major_number = major(dirent->device),
            .minor_number = minor(dirent->device),
        };
        return block_read(dev, offset, buffer, size);
    }
    if (dirent->type == DT_CHAR) {
        char_device_t dev = {
            .major_number = major(dirent->device),
            .minor_number = minor(dirent->device),
        };
        return char_read(dev, offset, buffer, size);
    }
    ramfs_data_t* current = dirent->first_block;
    if (offset >= dirent->file_size) return 0;
    if (offset + size > dirent->file_size) size = dirent->file_size - offset;
    size_t blocks_skipped = offset / RAMFS_BLOCK_SIZE;
    for (size_t i = 0; i < blocks_skipped; i++) current = current->next;
    size_t bytes_read = 0;
    int block_offset = offset % RAMFS_BLOCK_SIZE;
    while (bytes_read < size && current) {
        int to_read = RAMFS_BLOCK_SIZE - block_offset;
        if (to_read > size - bytes_read) to_read = size - bytes_read;
        memcpy(buffer + bytes_read, current->data + block_offset, to_read);
        block_offset = 0;
        bytes_read += to_read;
        current = current->next;
    }
    return bytes_read;
}

int ramfs_delete(const char *path) {
    if (mount->read_only) return -EROFS;
    ramfs_dirent_t* dirent;
    int res = ramfs_get_dirent(path, &dirent);
    if (res < 0) return res;
    dirent->to_delete = 1;
    if (dirent->open_count > 0) return 0;
    return ramfs_free_dirent(dirent);
}

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

static int ramfs_add_dirent(const char* path, ramfs_dirent_t* dirent) {
    if (mount->read_only) return -EROFS;
    ramfs_dirent_t* parent_dir;
    int res = ramfs_get_dirent(path, &parent_dir);
    if (res < 0) return res;
    if (parent_dir->type != DT_DIR) return -ENOTDIR;
    dirent->parent = parent_dir;
    dirent->next = parent_dir->child;
    parent_dir->child = dirent;
    if (dirent->next) dirent->next->previous = dirent;
    dirent->previous = NULL;
    dirent->child = NULL;
    return 0;
}

int ramfs_mknod(const char *path, uint32_t type, dev_t dev) {
    if (mount->read_only) return -EROFS;
    if (type != DT_BLOCK && type != DT_CHAR) return -EINVAL;
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    size_t slash = len;
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }
    char dirname[256];
    char filename[256];
    if (slash == 0) {
        dirname[0] = '\0';
    } else {
        size_t dlen = slash - 1;
        memcpy(dirname, path, dlen);
        dirname[dlen] = '\0';
    }
    size_t flen = len - slash;
    memcpy(filename, path + slash, flen);
    filename[flen] = '\0';

    ramfs_dirent_t* dirent = kmalloc(sizeof(ramfs_dirent_t));
    memcpy(dirent->name, filename, 255);
    dirent->name[255] = 0;
    dirent->type = type;
    dirent->device = dev;

    return ramfs_add_dirent(dirname, dirent);
}

int ramfs_create_file(const char *path) {
    if (mount->read_only) return -EROFS;
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    size_t slash = len;
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }
    char dirname[256];
    char filename[256];
    if (slash == 0) {
        dirname[0] = '\0';
    } else {
        size_t dlen = slash - 1;
        memcpy(dirname, path, dlen);
        dirname[dlen] = '\0';
    }
    size_t flen = len - slash;
    memcpy(filename, path + slash, flen);
    filename[flen] = '\0';

    ramfs_dirent_t* dirent = kmalloc(sizeof(ramfs_dirent_t));
    memcpy(dirent->name, filename, 255);
    dirent->name[255] = 0;
    dirent->type = DT_FILE;

    return ramfs_add_dirent(dirname, dirent);
}

int ramfs_create_directory(const char *path) {
    if (mount->read_only) return -EROFS;
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    size_t slash = len;
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }
    char dirname[256];
    char filename[256];
    if (slash == 0) {
        dirname[0] = '\0';
    } else {
        size_t dlen = slash - 1;
        memcpy(dirname, path, dlen);
        dirname[dlen] = '\0';
    }
    size_t flen = len - slash;
    memcpy(filename, path + slash, flen);
    filename[flen] = '\0';

    ramfs_dirent_t* dirent = kmalloc(sizeof(ramfs_dirent_t));
    memcpy(dirent->name, filename, 255);
    dirent->name[255] = 0;
    dirent->type = DT_DIR;

    return ramfs_add_dirent(dirname, dirent);
}

int ramfs_write(uint64_t handle, const uint8_t *buffer, size_t offset, size_t size) {
    if (mount->read_only) return -EROFS;
    ramfs_dirent_t* dirent = (ramfs_dirent_t*)handle;
    if (dirent->type == DT_DIR) return -EISDIR;
    if (dirent->type == DT_BLOCK) {
        block_device_t dev = {
            .major_number = major(dirent->device),
            .minor_number = minor(dirent->device),
        };
        return block_write(dev, offset, buffer, size);
    }
    if (dirent->type == DT_CHAR) {
        char_device_t dev = {
            .major_number = major(dirent->device),
            .minor_number = minor(dirent->device),
        };
        return char_write(dev, offset, buffer, size);
    }
    if (!dirent->first_block) {
        dirent->first_block = kmalloc(sizeof(ramfs_data_t));
        dirent->first_block->next = NULL;
    }
    ramfs_data_t* current = dirent->first_block;
    size_t blocks_skipped = offset / RAMFS_BLOCK_SIZE;
    for (size_t i = 0; i < blocks_skipped; i++) {
        if (!current->next) {
            current->next = kmalloc(sizeof(ramfs_data_t));
            current->next->next = NULL;
        }
        current = current->next;
    }
    size_t bytes_written = 0;
    int block_offset = offset % RAMFS_BLOCK_SIZE;
    while (bytes_written < size) {
        int to_write = RAMFS_BLOCK_SIZE - block_offset;
        if (to_write > size - bytes_written) to_write = size - bytes_written;
        memcpy(current->data + block_offset, buffer, to_write);
        bytes_written += to_write;
        buffer += to_write;
        block_offset = 0;
        if (!current->next) {
            current->next = kmalloc(sizeof(ramfs_data_t));
            current->next->next = NULL;
        }
        current = current->next;
    }
    if (dirent->file_size < offset + size) dirent->file_size = offset + size;
    return bytes_written;
}

int ramfs_rename(const char *old_path, const char *new_path) {
    if (mount->read_only) return -EROFS;
    ramfs_dirent_t* dirent;
    int res = ramfs_get_dirent(old_path, &dirent);
    if (res < 0) return res;

    if (dirent->previous) {
        dirent->previous->next = dirent->next;
    }
    if (dirent->parent->child == dirent) {
        dirent->parent->child = dirent->next;
    }
    if (dirent->next) {
        dirent->next->previous = dirent->previous;
    }

    size_t len = strlen(new_path);
    while (len > 0 && new_path[len - 1] == '/') {
        len--;
    }
    size_t slash = len;
    while (slash > 0 && new_path[slash - 1] != '/') {
        slash--;
    }
    char dirname[256];
    if (slash == 0) {
        dirname[0] = '\0';
    } else {
        size_t dlen = slash - 1;
        memcpy(dirname, new_path, dlen);
        dirname[dlen] = '\0';
    }
    size_t flen = len - slash;
    memcpy(dirent->name, new_path + slash, flen);
    dirent->name[flen] = '\0';

    return ramfs_add_dirent(dirname, dirent);
}

int ramfs_stat(uint64_t handle, stat_t *out) {
    ramfs_dirent_t* dirent = (ramfs_dirent_t*)handle;

    memset(out, 0, sizeof(stat_t));
    out->type = dirent->type;
    out->size = dirent->file_size;
    out->rdev = dirent->device;
    // Timestamps will be added with RTC
    return 0;
}

int ramfs_check(block_device_t block) {
    return 1;
}

void* ramfs_mount(block_device_t block, int flags) {
    mount = kmalloc(sizeof(ramfs_mount_t));
    mount->root.type = DT_DIR;
    mount->read_only = flags & FLAG_READ_ONLY;
    return mount;
}

void ramfs_select(void* data) {
    mount = data;
}

void ramfs_register() {
    filesystem_t ramfs = {0};

    memcpy(ramfs.name, "ramfs", 6);
    ramfs.case_sensitive = 1;
    ramfs.requires_backing = 0;

    ramfs.check = ramfs_check;
    ramfs.mount = ramfs_mount;
    ramfs.select = ramfs_select;

    ramfs.lookup = ramfs_lookup;
    ramfs.open = ramfs_open;
    ramfs.close = ramfs_close;

    ramfs.readdir = ramfs_readdir;
    ramfs.read = ramfs_read;
    ramfs.remove = ramfs_delete;
    ramfs.mknod = ramfs_mknod;
    ramfs.create_file = ramfs_create_file;
    ramfs.create_directory = ramfs_create_directory;
    ramfs.write = ramfs_write;
    ramfs.rename = ramfs_rename;
    ramfs.stat = ramfs_stat;

    register_filesystem(ramfs);
}
