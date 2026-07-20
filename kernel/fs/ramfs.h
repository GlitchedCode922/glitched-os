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
    dev_t device;
    uint64_t file_size;
    uint64_t open_count;
    uint64_t mtime;
    uint64_t ctime;
    uint64_t btime;
    int to_delete;
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

int ramfs_lookup(const char* path, uint64_t* handle);
int ramfs_open(uint64_t handle);
int ramfs_close(uint64_t handle);

int ramfs_readdir(uint64_t handle, int index, dirent_t* out);
int64_t ramfs_read(uint64_t handle, uint8_t* buffer, size_t offset, size_t size);
int ramfs_delete(const char* path);
int ramfs_create_file(const char* path);
int ramfs_create_directory(const char* path);
int64_t ramfs_write(uint64_t handle, const uint8_t *buffer, size_t offset, size_t size);
int ramfs_rename(const char* old_path, const char* new_path);
int ramfs_stat(uint64_t handle, stat_t* out);
int ramfs_mknod(const char* path, uint32_t type, dev_t dev);

int ramfs_check(block_device_t block);
void* ramfs_mount(block_device_t block, int flags);
int ramfs_unmount(void* data);
void ramfs_select(void* data);

void ramfs_register();
