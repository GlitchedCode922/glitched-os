#pragma once
#include "../vfs.h"
#include <stdint.h>

int devfs_readdir(const char* path, int index, dirent_t* out);
int devfs_read(const char* path, uint8_t* buffer, size_t offset, size_t size);
int devfs_delete(const char* path);
int devfs_create_file(const char* path);
int devfs_create_directory(const char* path);
int devfs_write(const char *path, const uint8_t *buffer, size_t offset, size_t size);
int devfs_rename(const char* old_path, const char* new_path);
int devfs_stat(const char* path, stat_t* out);
int devfs_mknod(const char* path, uint32_t type, dev_t dev);

void devfs_register();
