#pragma once
#include "../vfs.h"
#include <stdint.h>

int devfs_lookup(const char* path, uint64_t* handle);
int devfs_open(uint64_t handle);
int devfs_close(uint64_t handle);

int devfs_readdir(uint64_t handle, int index, dirent_t* out);
int64_t devfs_read(uint64_t handle, uint8_t* buffer, size_t offset, size_t size);
int devfs_delete(const char* path);
int devfs_create_file(const char* path);
int devfs_create_directory(const char* path);
int64_t devfs_write(uint64_t handle, const uint8_t *buffer, size_t offset, size_t size);
int devfs_rename(const char* old_path, const char* new_path);
int devfs_stat(uint64_t handle, stat_t* out);
int devfs_mknod(const char* path, uint32_t type, dev_t dev);

void devfs_register();
