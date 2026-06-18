#include "devfs.h"
#include "ramfs.h"
#include "../memory/mman.h"

static void* ramfs_instance;

int devfs_readdir(const char* path, int index, dirent_t* out) {
    ramfs_select(ramfs_instance);
    return ramfs_readdir(path, index, out);
}

int devfs_read(const char* path, uint8_t* buffer, size_t offset, size_t size) {
    ramfs_select(ramfs_instance);
    return ramfs_read(path, buffer, offset, size);
}

int devfs_delete(const char* path) {
    ramfs_select(ramfs_instance);
    return ramfs_delete(path);
}

int devfs_create_file(const char* path) {
    ramfs_select(ramfs_instance);
    return ramfs_create_file(path);
}

int devfs_create_directory(const char* path) {
    ramfs_select(ramfs_instance);
    return ramfs_create_directory(path);
}

int devfs_write(const char *path, const uint8_t *buffer, size_t offset, size_t size) {
    ramfs_select(ramfs_instance);
    return ramfs_write(path, buffer, offset, size);
}

int devfs_rename(const char* old_path, const char* new_path) {
    ramfs_select(ramfs_instance);
    return ramfs_rename(old_path, new_path);
}

int devfs_stat(const char* path, stat_t* out) {
    ramfs_select(ramfs_instance);
    return ramfs_stat(path, out);
}

int devfs_mknod(const char* path, uint32_t type, dev_t dev) {
    ramfs_select(ramfs_instance);
    return ramfs_mknod(path, type, dev);
}

int devfs_check(block_device_t block) {
    return 1;
}

void* devfs_mount(block_device_t block, int flags) {
    return NULL;
}

void devfs_select(void* data) {
}

void devfs_register() {
    ramfs_instance = ramfs_mount((block_device_t){0}, 0);
    filesystem_t devfs = {0};

    memcpy(devfs.name, "devfs", 6);
    devfs.case_sensitive = 1;
    devfs.requires_backing = 0;

    devfs.check = devfs_check;
    devfs.mount = devfs_mount;
    devfs.select = devfs_select;

    devfs.readdir = devfs_readdir;
    devfs.read = devfs_read;
    devfs.remove = devfs_delete;
    devfs.mknod = devfs_mknod;
    devfs.create_file = devfs_create_file;
    devfs.create_directory = devfs_create_directory;
    devfs.write = devfs_write;
    devfs.rename = devfs_rename;
    devfs.stat = devfs_stat;

    register_filesystem(devfs);
}

