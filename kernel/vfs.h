#pragma once
#include "drivers/block.h"
#include <stdint.h>
#include <stddef.h>

#define FLAG_READ_ONLY 0x01
#define MAX_PATH 2048

typedef uint64_t dev_t;

enum {
    DT_UNKNOWN = 0,
    DT_FILE = 1,
    DT_DIR = 2,
    DT_BLOCK = 3,
};

typedef struct {
    char name[256];
    uint32_t type;
} __attribute__((packed)) dirent_t;

typedef struct {
    uint64_t size;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t btime;
    uint32_t type;
    dev_t rdev;
} __attribute__((packed)) stat_t;

typedef struct {
    char name[32];          // Name of the filesystem
    int case_sensitive;     // Whether the filesystem is case-sensitive
    int requires_backing;

    int (*check)(block_device_t device);
    void* (*mount)(block_device_t device, int flags);
    void (*select)(void* data);

    int (*readdir)(const char *path, int index, dirent_t* out); // List directory contents
    int (*read)(const char *path, uint8_t *buffer, size_t offset, size_t size); // Read from a file
    int (*write)(const char *path, const uint8_t *buffer, size_t offset, size_t size); // Write to a file
    int (*remove)(const char *path); // Delete a file or directory
    int (*rename)(const char *old_path, const char *new_path); // Rename a file or directory 
    int (*create_file)(const char *path); // Create a new file
    int (*create_directory)(const char *path); // Create a new directory
    int (*stat)(const char *path, stat_t *out);
    int (*mknod)(const char* path, uint32_t type, dev_t dev);
} filesystem_t;

typedef struct mountpoint {
    char mount_point[MAX_PATH];  // Relative mount point path
    int type;                    // Filesystem type
    void* fs_data;
    struct mountpoint* parent;   // Pointer to the parent mount point
    struct mountpoint* next;     // Pointer to the next mount point
    struct mountpoint* children; // Pointer to the first child mount point (for nested mounts)
} mountpoint_t;

int register_filesystem(filesystem_t fs);
int mount_filesystem(const char* source, const char* target, const char* type, int flags);
int mount_root_filesystem(const char* device, const char* type, int flags);
int unmount_filesystem(const char *path);
int unmount_all_filesystems();
int readdir(const char *path, int index, dirent_t* out);
int stat(const char* path, stat_t* out);
int read_file(const char *path, uint8_t *buffer, size_t offset, size_t size);
int write_file(const char *path, const uint8_t *buffer, size_t offset, size_t size);
int remove_file(const char *path);
int create_file(const char *path);
int create_directory(const char *path);
int rename_file(const char *old_path, const char *new_path);
void register_intree_filesystems();
void getcwd(char* buffer, size_t len);
int chdir(char* path);
int mknod(const char* path, uint32_t type, dev_t dev);
dev_t makedev(uint32_t major, uint32_t minor);
uint32_t major(dev_t device);
uint32_t minor(dev_t device);
