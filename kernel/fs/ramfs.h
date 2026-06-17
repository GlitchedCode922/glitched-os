#pragma once
#include "../vfs.h"
#include <stdint.h>

typedef struct ramfs_data {
    char data[4096 - sizeof(uintptr_t)]; // Fit struct in 1 page to reduce fragmentation
    struct ramfs_data* next;
} __attribute__((packed)) ramfs_data_t;

typedef struct ramfs_dirent {
    char name[256];
    int type;
    uint64_t file_size;
    ramfs_data_t* first_block;
    struct ramfs_dirent* previous;
    struct ramfs_dirent* next;
    struct ramfs_dirent* parent;
    struct ramfs_dirent* child;
} ramfs_dirent_t;

typedef struct {
    ramfs_dirent_t root;
    int read_only;
} ramfs_mount_t;

#define RAMFS_BLOCK_SIZE (4096 - sizeof(uintptr_t))

int ramfs_readdir(const char* path, int index, dirent_t* out);
int ramfs_read(const char* path, uint8_t* buffer, size_t offset, size_t size);
int ramfs_delete(const char* path);
int ramfs_create_file(const char* path);
int ramfs_create_directory(const char* path);
int ramfs_write(const char *path, const uint8_t *buffer, size_t offset, size_t size);
int ramfs_rename(const char* old_path, const char* new_path);
int ramfs_stat(const char* path, stat_t* out);

void ramfs_register();
