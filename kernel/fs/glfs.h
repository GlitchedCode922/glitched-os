#pragma once
#include "../drivers/block.h"
#include "../vfs.h"
#include <stdint.h>

int glfs_glue_readdir(const char* path, int index, dirent_t* out);
int glfs_glue_read(const char* path, uint8_t* buffer, size_t offset, size_t size);
int glfs_glue_delete(const char* path);
int glfs_glue_create_file(const char* path);
int glfs_glue_create_directory(const char* path);
int glfs_glue_write(const char *path, const uint8_t *buffer, size_t offset, size_t size);
int glfs_glue_rename(const char* old_path, const char* new_path);
int glfs_glue_stat(const char* path, stat_t* out);
int glfs_glue_mknod(const char* path, uint32_t type, dev_t dev);

void glfs_register();
