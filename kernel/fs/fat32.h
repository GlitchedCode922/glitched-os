#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct __attribute__((packed)) {
    uint8_t jmp[3];          // Jump instruction to boot code
    char oem_name[8];        // OEM name
    uint16_t bytes_per_sector; // Bytes per sector
    uint8_t sectors_per_cluster; // Sectors per cluster
    uint16_t reserved_sectors; // Reserved sectors count
    uint8_t num_fats;         // Number of FATs
    uint16_t root_entries;     // Number of root directory entries
    uint16_t total_sectors_16; // Total sectors (if < 65536)
    uint8_t media_type;       // Media type
    uint16_t fat_size_16;     // Size of one FAT in sectors (if < 65536)
    uint16_t sectors_per_track; // Sectors per track
    uint16_t heads;           // Number of heads
    uint32_t hidden_sectors;  // Hidden sectors count
    uint32_t total_sectors_32; // Total sectors (if >= 65536)
    uint32_t fat_size_32; // Size of one FAT in sectors (if >= 65536)
    uint16_t flags;       // Flags
    uint16_t version;     // FAT version
    uint32_t root_cluster; // Cluster number of the root directory
    uint16_t fs_info;    // FSINFO sector number
    uint16_t backup_boot_sector; // Backup boot sector number
    uint8_t reserved[12]; // Reserved bytes
    uint8_t drive_number; // Drive number
    uint8_t reserved1;    // Reserved byte
    uint8_t boot_signature; // Boot signature (0x28/0x29)
    uint32_t volume_id;   // Volume ID
    char volume_label[11]; // Volume label
    char file_system_type[8]; // File system type (e.g., "FAT32   ")
    uint8_t boot_code[420]; // Boot code (up to 420 bytes)
    uint16_t boot_sector_signature; // Boot sector signature (0x55AA)
} bpb_t;

typedef struct __attribute__((packed)) {
    uint32_t signature; // Signature (0x41615252)
    uint8_t reserved[480]; // Reserved bytes
    uint32_t signature2;
    uint32_t free_clusters; // Free clusters count
    uint32_t next_free_cluster; // Next free cluster number
    uint8_t reserved2[12]; // Reserved bytes
    uint32_t signature3;
} fsinfo_t;

typedef struct __attribute__((packed)) {
    char name[11];    // File name (8.3 format)
    uint8_t attributes; // File attributes (e.g., read-only, hidden, system, volume label, directory, archive)
    uint8_t reserved;  // Reserved byte
    uint8_t creation_time_tenth; // Creation time (tenth of a second)
    uint16_t creation_time; // Creation time (hours, minutes, seconds)
    uint16_t creation_date; // Creation date (year, month, day)
    uint16_t last_access_date; // Last access date
    uint16_t first_cluster_high; // High 16 bits of the first cluster number
    uint16_t last_modification_time; // Last modification time
    uint16_t last_modification_date; // Last modification date
    uint16_t first_cluster_low; // Low 16 bits of the first cluster number
    uint32_t file_size; // File size in bytes
} dirent_t;

#define DIRENT_READ_ONLY 0x01
#define DIRENT_HIDDEN 0x02
#define DIRENT_SYSTEM 0x04
#define DIRENT_VOLUME_LABEL 0x08
#define DIRENT_DIRECTORY 0x10
#define DIRENT_ARCHIVE 0x20
#define DIRENT_LONG_NAME 0x0F // Long file name entry

#define CLUSTER_CHAIN_END 0x0FFFFFFF
#define CLUSTER_FREE 0x00000000

void select_partition(uint8_t disk, uint8_t partition);
void init_fat32(uint8_t disk, uint8_t partition);
bpb_t get_bpb();
fsinfo_t get_fsinfo(uint32_t fsinfo_sector);
uint32_t get_next_cluster(uint32_t cluster);
uint32_t get_cluster_size();
int lsdir(const char* path, char* element, uint64_t element_index);
int file_exists(const char* path);
int is_directory(const char* path);
int read_from_file(const char* path, uint8_t* buffer, size_t offset, size_t size);
int delete_entry(const char* path);
int add_dirent(const char* path, dirent_t dirent);
uint32_t get_free_cluster();
uint32_t update_free_cluster();
void create_file(const char* path);
