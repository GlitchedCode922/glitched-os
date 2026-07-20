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
    DT_CHAR = 4,
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
    int (*unmount)(void* data);
    void (*select)(void* data);

    int (*lookup)(const char* path, uint64_t* handle);
    int (*open)(uint64_t handle);
    int (*close)(uint64_t handle);

    int (*readdir)(uint64_t handle, int index, dirent_t* out); // List directory contents
    int64_t (*read)(uint64_t handle, uint8_t *buffer, size_t offset, size_t size); // Read from a file
    int64_t (*write)(uint64_t handle, const uint8_t *buffer, size_t offset, size_t size); // Write to a file
    int (*stat)(uint64_t handle, stat_t *out);
    int (*mknod)(const char* path, uint32_t type, dev_t dev);
    int (*create_file)(const char *path); // Create a new file
    int (*create_directory)(const char *path); // Create a new directory
    int (*remove)(const char *path); // Delete a file or directory
    int (*rename)(const char *old_path, const char *new_path); // Rename a file or directory
    int (*link)(uint64_t handle, const char *link);
} filesystem_t;

typedef struct mountpoint {
    char mount_point[MAX_PATH];  // Relative mount point path
    int type;                    // Filesystem type
    int refcount;
    void* fs_data;
    struct mountpoint* parent;   // Pointer to the parent mount point
    struct mountpoint* next;     // Pointer to the next mount point
    struct mountpoint* children; // Pointer to the first child mount point (for nested mounts)
} mountpoint_t;

typedef struct {
    mountpoint_t* mountpoint;
    uint64_t handle;
} file_handle_t;

int register_filesystem(filesystem_t fs);
int mount_filesystem(const char* source, const char* target, const char* type, int flags);
int mount_root_filesystem(const char* device, int flags);
int unmount_filesystem(const char *path);
int lookup(const char *path, file_handle_t* file);
int open(const char *path, file_handle_t* open_file);
int close(file_handle_t open_file);
int readdir(file_handle_t file, int index, dirent_t* out);
int stat_handle(file_handle_t file, stat_t* out);
int stat(const char* path, stat_t* out);
int64_t read_file(file_handle_t file, uint8_t *buffer, size_t offset, size_t size);
int64_t write_file(file_handle_t file, const uint8_t *buffer, size_t offset, size_t size);
int remove_file(const char *path);
int create_file(const char *path);
int create_directory(const char *path);
int rename_file(const char *old_path, const char *new_path);
void register_intree_filesystems();
void getcwd(char* buffer, size_t len);
int chdir(char* path);
int mknod(const char* path, uint32_t type, dev_t dev);
int link(const char *file, const char *link);
int ioctl(file_handle_t file, uint64_t request, uint64_t arg);
int clone_file_handle(file_handle_t file);
dev_t makedev(uint32_t major, uint32_t minor);
uint32_t major(dev_t device);
uint32_t minor(dev_t device);
