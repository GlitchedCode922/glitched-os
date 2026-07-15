#include "devfs.h"
#include "ramfs.h"
#include "../memory/mman.h"
#include <stdint.h>

static void* ramfs_instance;

int devfs_lookup(const char* path, uint64_t* handle) {
    ramfs_select(ramfs_instance);
    return ramfs_lookup(path, handle);
}

int devfs_open(uint64_t handle) {
    ramfs_select(ramfs_instance);
    return ramfs_open(handle);
}

int devfs_close(uint64_t handle) {
    ramfs_select(ramfs_instance);
    return ramfs_close(handle);
}

int devfs_readdir(uint64_t handle, int index, dirent_t* out) {
    ramfs_select(ramfs_instance);
    return ramfs_readdir(handle, index, out);
}

int devfs_read(uint64_t handle, uint8_t* buffer, size_t offset, size_t size) {
    ramfs_select(ramfs_instance);
    return ramfs_read(handle, buffer, offset, size);
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

int devfs_write(uint64_t handle, const uint8_t *buffer, size_t offset, size_t size) {
    ramfs_select(ramfs_instance);
    return ramfs_write(handle, buffer, offset, size);
}

int devfs_rename(const char* old_path, const char* new_path) {
    ramfs_select(ramfs_instance);
    return ramfs_rename(old_path, new_path);
}

int devfs_stat(uint64_t handle, stat_t* out) {
    ramfs_select(ramfs_instance);
    return ramfs_stat(handle, out);
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

int devfs_unmount(void* data) {
    return 0;
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
    devfs.unmount = devfs_unmount;
    devfs.select = devfs_select;

    devfs.lookup = devfs_lookup;
    devfs.open = devfs_open;
    devfs.close = devfs_close;

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

