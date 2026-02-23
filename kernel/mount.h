#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char name[32];          // Name of the filesystem

    int (*check)(uint8_t drive, uint8_t partition);  // Function to check if the filesystem is valid
    void (*select)(uint8_t drive, uint8_t partition);
    void (*set_read_only)(uint8_t read_only); // Set the filesystem to read-only mode

    int (*list)(const char *path, char *element, uint64_t element_index); // List directory contents
    int (*exists)(const char *path); // Check if a file or directory exists
    int (*is_directory)(const char *path); // Check if a path is a directory
    uint64_t (*get_file_size)(const char *path); // Get the size of a file
    int (*read)(const char *path, uint8_t *buffer, size_t offset, size_t size); // Read from a file
    int (*write)(const char *path, const uint8_t *buffer, size_t offset, size_t size); // Write to a file
    int (*remove)(const char *path); // Delete a file or directory
    int (*create_file)(const char *path); // Create a new file
    int (*create_directory)(const char *path); // Create a new directory
    int (*get_creation_time)(const char *path, uint64_t *timestamp); // Get file creation time
    int (*get_last_modification_time)(const char *path, uint64_t *timestamp); // Get last modification time
} filesystem_t;

typedef struct {
    int drive;              // Drive number
    int partition;          // Partition number
    int flags;             // Flags for mounting (e.g., read-only)
    char mount_point[256]; // Mount point path
    char type[32];         // Filesystem type (e.g., "FAT32", "EXT4")
} mountpoint_t;

#define FLAG_READ_ONLY 0x01
#define MAX_PATH 1024

int register_filesystem(filesystem_t fs);
int mount_filesystem(const char *path, const char *type, int drive, int partition, int flags);
int unmount_filesystem(const char *path);
int unmount_all_filesystems();
int list_directory(const char *path, char *element, uint64_t element_index);
int exists(const char *path);
int is_directory(const char *path);
uint64_t get_file_size(const char *path);
int read_file(const char *path, uint8_t *buffer, size_t offset, size_t size);
int write_file(const char *path, const uint8_t *buffer, size_t offset, size_t size);
int remove_file(const char *path);
int create_file(const char *path);
int create_directory(const char *path);
int get_creation_time(const char *path, uint64_t *timestamp);
int get_last_modification_time(const char *path, uint64_t *timestamp);
void register_intree_filesystems();
void getcwd(char* buffer, size_t len);
int chdir(char* path);
