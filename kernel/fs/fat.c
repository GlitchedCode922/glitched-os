#include "fat.h"
#include "../drivers/partitions.h"
#include "../memory/mman.h"
#include "../mount.h"
#include "../console.h"
#include <stddef.h>
#include <stdint.h>

static uint8_t active_disk;
static uint8_t active_partition;
static bpb_t bpb;
static fsinfo_t fsinfo;
static uint8_t read_only = 0;
static uint32_t fat_size = 0;
static int initialized = 0;
static uint32_t last_free = 0;

static void get_wall_clock_time(uint32_t* year, uint32_t* month, uint32_t* day,
                        uint32_t* hour, uint32_t* minute, uint32_t* second) {
    // This function is a placeholder. It will be added with the RTC driver in timer.c.
    *year = 2026;
    *month = 2;
    *day = 1;
    *hour = 17;
    *minute = 0;
    *second = 0;
}

uint64_t fat_timestamp_to_unix(uint16_t fat_date, uint16_t fat_time) {
    // Convert FAT date and time to Unix timestamp
    int year = ((fat_date >> 9) & 0x7F) + 1980;
    int month = (fat_date >> 5) & 0x0F;
    int day = fat_date & 0x1F;
    int hour = (fat_time >> 11) & 0x1F;
    int minute = (fat_time >> 5) & 0x3F;
    int second = (fat_time & 0x1F) * 2;

    // Simple conversion to Unix timestamp (not accounting for leap years, etc.)
    uint64_t days = (year - 1970) * 365 + (month - 1) * 30 + (day - 1); // Approximation
    uint64_t timestamp = days * 86400 + hour * 3600 + minute * 60 + second;
    return timestamp;
}

void wall_clock_to_fat_timestamp(int year, int month, int day, int hour, int min, int sec, uint16_t *fat_date, uint16_t *fat_time) {
    // FAT stores year as years since 1980
    if (year < 1980) year = 1980;
    int year_since_1980 = year - 1980;

    // Pack date field
    *fat_date = ((year_since_1980 & 0x7F) << 9)  // bits 9-15
                | ((month & 0x0F) << 5)          // bits 5-8
                | (day & 0x1F);                  // bits 0-4

    // Pack time field
    *fat_time = ((hour & 0x1F) << 11)            // bits 11-15
                | ((min & 0x3F) << 5)            // bits 5-10
                | ((sec / 2) & 0x1F);           // bits 0-4 (seconds / 2)
}

void fat_timestamp_to_wall_clock(uint16_t fat_date, uint16_t fat_time, int *year, int *month, int *day, int *hour, int *min, int *sec) {
    // Extract year, month, day from fat_date
    int year_since_1980 = (fat_date >> 9) & 0x7F; // bits 9-15
    *year = year_since_1980 + 1980; // FAT year is years since 1980
    *month = (fat_date >> 5) & 0x0F; // bits 5-8
    *day = fat_date & 0x1F; // bits 0-4

    // Extract hour, minute, second from fat_time
    *hour = (fat_time >> 11) & 0x1F; // bits 11-15
    *min = (fat_time >> 5) & 0x3F; // bits 5-10
    *sec = (fat_time & 0x1F) * 2; // bits 0-4, seconds are stored as half-seconds in FAT
}

void fat_init(uint8_t disk, uint8_t partition) {
    if (initialized && active_disk == disk && active_partition == partition) {
        return; // Already initialized
    }
    active_disk = disk;
    active_partition = partition;
    bpb = fat_get_bpb();
    fsinfo = fat_get_fsinfo();
    fat_size = (bpb.fat_size_16 != 0) ? bpb.fat_size_16 : bpb.fat_size_32;
    fat_compute_free_cluster();
    initialized = 1;
}

int is_fat_partition(uint8_t disk, uint8_t partition) {
    active_disk = disk;
    active_partition = partition;
    bpb_t local_bpb = fat_get_bpb();
    char f32_signature[8] = "FAT32   ";
    return memcmp(local_bpb.file_system_type, f32_signature, 8);
}

void fat_select_partition(uint8_t disk, uint8_t partition) {
    active_disk = disk;
    active_partition = partition;
}

void fat_set_read_only(uint8_t ro) {
    read_only = ro;
}

bpb_t fat_get_bpb() {
    bpb_t bpb;
    read_sectors_relative(active_disk, active_partition, 0, (uint8_t*)&bpb, 1);
    return bpb;
}

fsinfo_t fat_get_fsinfo() {
    fsinfo_t fsinfo;
    read_sectors_relative(active_disk, active_partition, bpb.fs_info, (uint8_t*)&fsinfo, 1);
    return fsinfo;
}

uint32_t read_fat(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = bpb.reserved_sectors + (fat_offset / bpb.bytes_per_sector);
    uint32_t ent_offset = fat_offset % bpb.bytes_per_sector;

    uint8_t sector_buffer[512];
    read_sectors_relative(active_disk, active_partition, fat_sector, sector_buffer, 1);

    return *(uint32_t*)&sector_buffer[ent_offset];
}

void write_fat(uint32_t cluster, uint32_t content) {
    // Update all FAT copies
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = bpb.reserved_sectors + (fat_offset / bpb.bytes_per_sector);
    uint32_t ent_offset = fat_offset % bpb.bytes_per_sector;

    for (uint8_t i = 0; i < bpb.num_fats; i++) {
        uint8_t sector_buffer[512];
        read_sectors_relative(active_disk, active_partition, (fat_sector + i * fat_size), sector_buffer, 1);

        uint32_t *entry = (uint32_t*)&sector_buffer[ent_offset];
        uint32_t old = *entry;
        uint32_t newval = (old & 0xF0000000) | (content & 0x0FFFFFFF);
        *entry = newval;
        write_sectors_relative(active_disk, active_partition, (fat_sector + i * fat_size), sector_buffer, 1);
    }
}

uint64_t get_first_cluster_sector(uint32_t cluster) {
    uint32_t first_data_sector = bpb.reserved_sectors + (bpb.num_fats * fat_size);
    uint32_t first_sector_of_cluster = ((cluster - 2) * bpb.sectors_per_cluster) + first_data_sector;
    return first_sector_of_cluster;
}

void normalize_fat_path(const char *input_path, char *output_path) {
    // Convert to uppercase and remove leading, consecutive and trailing slashes
    int i = 0, j = 0;
    while (input_path[i] == '/') i++;
    while (input_path[i]) {
        char c = input_path[i];
        if (c >= 'a' && c <= 'z') {
            c -= 32; // Convert to uppercase
        }
        if (c == '/') {
            // Remove leading and consecutive slashes
            if (j > 0 && output_path[j - 1] == '/') {
                i++;
                continue;
            }
            output_path[j++] = c;
        } else {
            output_path[j++] = c;
        }
        i++;
    }
    // Remove trailing slash
    if (j > 0 && output_path[j - 1] == '/') {
        j--;
    }
    output_path[j] = '\0';
}

void readable_to_8d3(char path[12]) {
    // Convert a readable filename to 8.3 format
    char name[8] = { ' ' , ' ' , ' ' , ' ' , ' ' , ' ' , ' ' , ' ' };
    char ext[3] = { ' ' , ' ' , ' ' };
    int i = 0, j = 0;
    // Extract name
    while (i < 12 && path[i] != '.' && path[i] != ' ' && path[i] != '\0') {
        if (j < 8) {
            name[j++] = path[i];
        }
        i++;
    }
    // If there's an extension
    if (path[i] == '.') {
        i++;
        j = 0;
        while (i < 12 && path[i] != ' ' && path[i] != '\0') {
            if (j < 3) {
                ext[j++] = path[i];
            }
            i++;
        }
    }
    // Combine name and extension
    memcpy(path, name, 8);
    memcpy(path + 8, ext, 3);
}

void f8d3_to_readable(char path[12]) {
    char temp[12];
    int j = 0;

    // Copy name, skipping spaces
    for (int i = 0; i < 8 && j < 12; i++) {
        if (path[i] != ' ') {
            temp[j++] = path[i];
        }
    }

    // Check if there is an extension
    int ext_len = 0;
    for (int i = 8; i < 11; i++) {
        if (path[i] != ' ') ext_len++;
    }

    // Add dot + extension if it exists and we have space
    if (ext_len > 0 && j < 12) {
        temp[j++] = '.';
        for (int i = 8; i < 11 && j < 12; i++) {
            if (path[i] != ' ' && j < 12) {
                temp[j++] = path[i];
            }
        }
    }

    // Fill rest with nulls if we didn’t reach 12 chars
    for (; j < 12; j++) temp[j] = '\0';

    // Copy back to original array
    for (int i = 0; i < 12; i++) path[i] = temp[i];
}

uint32_t next_cluster(uint32_t current_cluster) {
    return read_fat(current_cluster) & 0x0FFFFFFF;
}

dirent_ref_t fat_get_dirent_ref(const char *path) {
    // Find the dirent_ref for the given path
    dirent_ref_t dirent_ref;
    dirent_ref.dirent.attributes = DIRENT_DIRECTORY; // Start assuming root is a directory
    dirent_ref.dirent.first_cluster_low = bpb.root_cluster & 0xFFFF;
    dirent_ref.dirent.first_cluster_high = (bpb.root_cluster >> 16) & 0xFFFF;
    char npath[512];
    normalize_fat_path(path, npath);
    path = npath; // Replace path with normalized for the rest of the function
    // Start from the root directory
    dirent_ref.found = 1;
    dirent_ref.cluster = bpb.root_cluster;
    char* path_ptr = (char*)path;
    while (*path_ptr) {
        // Check if previous component is a directory
        if ((dirent_ref.dirent.attributes & DIRENT_DIRECTORY) == 0) {
            // Not a directory
            dirent_ref.found = 0;
            dirent_ref.cluster = 0;
            dirent_ref.dirent = (dirent_t){0};
            break;
        }
        // Read the directory entries in the current cluster
        uint32_t cluster = dirent_ref.cluster;
        uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
        uint8_t cluster_buffer[bytes_per_cluster];
        read_sectors_relative(active_disk, active_partition, get_first_cluster_sector(cluster), cluster_buffer, bpb.sectors_per_cluster);

        // Search for the next component in the path
        char component[12]; // 8.3 format
        int comp_len = 0;
        while (*path_ptr && *path_ptr != '/' && comp_len < 12) {
            component[comp_len++] = *path_ptr++;
        }
        for (int i = comp_len; i < 12; i++) component[i] = ' ';
        readable_to_8d3(component);
        if (*path_ptr == '/') path_ptr++; // Skip the slash

        // Search for the component in the current directory (all clusters)
        int found = 0;
        uint32_t offset = 0;
        while (1) {
            if (offset >= bytes_per_cluster) {
                // Move to next cluster
                cluster = next_cluster(cluster);
                if (cluster >= CLUSTER_CHAIN_END) break; // End of cluster chain

                read_sectors_relative(active_disk, active_partition, get_first_cluster_sector(cluster), cluster_buffer, bpb.sectors_per_cluster);
                offset = 0;
            }
            dirent_t* dirent = (dirent_t*)&cluster_buffer[offset];
            if (dirent->name[0] == DIRENT_END) break; // No more entries
            if (dirent->name[0] == DIRENT_DELETED || dirent->attributes & DIRENT_VOLUME_LABEL) {
                offset += sizeof(dirent_t);
                continue; // Deleted or volume label entry
            }

            char dirent_name[11];
            memcpy(dirent_name, dirent->name, 11);

            if (memcmp(dirent_name, component, 11) == 0) {
                // Found the component
                dirent_ref.found = 1;
                dirent_ref.dirent = *dirent;
                dirent_ref.cluster = ((uint32_t)dirent->first_cluster_high << 16) | dirent->first_cluster_low;
                dirent_ref.position[0] = get_first_cluster_sector(cluster) + offset / 512;
                dirent_ref.position[1] = offset % 512;
                found = 1;
                break;
            }
            offset += sizeof(dirent_t);
        }
        if (!found) {
            // Component not found
            dirent_ref.found = 0;
            dirent_ref.cluster = 0;
            dirent_ref.dirent = (dirent_t){0};
            break;
        }
    }
    return dirent_ref;
}

int fat_list(const char *path, char element[13], uint64_t element_index) {
    // List the directory entries in the given path
    char npath[512];
    normalize_fat_path(path, npath);
    path = npath; // Replace path with normalized for the rest of the function

    // Start from the root directory
    uint32_t cluster = bpb.root_cluster;
    char* path_ptr = (char*)path;

    // Traverse to the target directory
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found || !(dirent_ref.dirent.attributes & DIRENT_DIRECTORY)) {
        element[0] = '\0'; // Directory not found
        return -1;
    }

    cluster = dirent_ref.cluster;
    uint64_t current_index = 0;
    while (cluster < CLUSTER_CHAIN_END) {
        uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
        uint8_t cluster_buffer[bytes_per_cluster];
        read_sectors_relative(active_disk, active_partition, get_first_cluster_sector(cluster), cluster_buffer, bpb.sectors_per_cluster);

        // Iterate through directory entries
        uint32_t offset = 0;
        while (offset < bytes_per_cluster) {
            dirent_t* dirent = (dirent_t*)&cluster_buffer[offset];
            if (dirent->name[0] == DIRENT_END) break; // No more entries
            if (dirent->name[0] == DIRENT_DELETED || dirent->attributes & DIRENT_VOLUME_LABEL) {
                offset += sizeof(dirent_t);
                continue; // Deleted or volume label entry
            }

            if (current_index == element_index) {
                // Found the requested element
                char name[13];
                memcpy(name, dirent->name, 12);
                f8d3_to_readable(name);
                name[12] = '\0'; // Null-terminate
                memcpy(element, name, 13);
                return 0;
            }
            current_index++;
            offset += sizeof(dirent_t);
        }
        cluster = next_cluster(cluster);
    }
    element[0] = '\0';
    return 1; // End of list
}

int fat_exists(const char* path) {
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    return dirent_ref.found;
}

int fat_is_directory(const char* path) {
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found) return 0;
    return (dirent_ref.dirent.attributes & DIRENT_DIRECTORY) != 0;
}

uint64_t fat_get_file_size(const char* path) {
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found) return 0;
    return dirent_ref.dirent.file_size;
}

int fat_read(const char *path, uint8_t *buffer, size_t offset, size_t size) {
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found) return -1; // File not found

    uint32_t file_cluster = ((uint32_t)dirent_ref.dirent.first_cluster_high << 16) | dirent_ref.dirent.first_cluster_low;
    uint32_t file_size = dirent_ref.dirent.file_size;

    if (offset >= file_size) return -2; // Offset beyond file size
    if (offset + size > file_size) size = file_size - offset; // Adjust size to read

    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t current_cluster = file_cluster;
    uint32_t cluster_offset = 0;
    size_t bytes_read = 0;

    // Skip clusters to reach the offset
    while (offset >= cluster_size) {
        current_cluster = next_cluster(current_cluster);
        if (current_cluster >= CLUSTER_CHAIN_END) return -3; // Reached end of cluster chain
        offset -= cluster_size;
    }
    cluster_offset = offset;

    // Read data
    while (bytes_read < size) {
        uint32_t first_sector = ((current_cluster - 2) * bpb.sectors_per_cluster) + bpb.reserved_sectors + (bpb.num_fats * fat_size);
        uint8_t cluster_buffer[cluster_size];
        read_sectors_relative(active_disk, active_partition, first_sector, cluster_buffer, bpb.sectors_per_cluster);

        while (cluster_offset < cluster_size && bytes_read < size) {
            buffer[bytes_read++] = cluster_buffer[cluster_offset++];
        }

        if (bytes_read < size) {
            current_cluster = next_cluster(current_cluster);
            if (current_cluster >= CLUSTER_CHAIN_END) break; // Reached end of cluster chain
            cluster_offset = 0;
        }
    }

    return bytes_read;
}

int fat_delete(const char* path) {
    if (read_only) {
        return -1; // Filesystem is read-only
    }
    // Normalize path
    char npath[512];
    normalize_fat_path(path, npath);
    path = npath; // Replace path with normalized for the rest of the function
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    // Go to position in dirent_ref and set first byte of name to 0xE5
    if (!dirent_ref.found) {
        return -2; // File not found
    }
    // Read the sector containing the dirent
    uint8_t sector_buffer[512];
    read_sectors_relative(active_disk, active_partition, dirent_ref.position[0], sector_buffer, 1);
    // Set first byte of name to 0xE5
    sector_buffer[dirent_ref.position[1]] = DIRENT_DELETED;
    write_sectors_relative(active_disk, active_partition, dirent_ref.position[0], sector_buffer, 1);
    // Free clusters used by the file
    uint32_t cluster = ((uint32_t)dirent_ref.dirent.first_cluster_high << 16) | dirent_ref.dirent.first_cluster_low;
    while (cluster < CLUSTER_CHAIN_END) {
        uint32_t next = next_cluster(cluster);
        // Mark cluster as free in FAT
        write_fat(cluster, CLUSTER_FREE);
        cluster = next;
    }
    return 0;
}

uint32_t fat_compute_free_cluster() {
    uint32_t total_clusters = (((bpb.total_sectors_16 != 0) ? bpb.total_sectors_16 : bpb.total_sectors_32) - (bpb.reserved_sectors + (bpb.num_fats * fat_size))) / bpb.sectors_per_cluster;
    for (uint32_t cluster = last_free; cluster < total_clusters + 2; cluster++) {
        if ((read_fat(cluster) & 0x0FFFFFFF) == CLUSTER_FREE) {
            last_free = cluster;
            return cluster;
        }
    }
    return 0; // No free cluster found
}

int fat_add_dirent(const char *path, dirent_t dirent) {
    if (read_only) {
        return -1; // Filesystem is read-only
    }
    // Add a dirent at the specified path
    char npath[512];
    normalize_fat_path(path, npath);
    path = npath; // Replace path with normalized for the rest of the function

    dirent_ref_t parent_dirent_ref = fat_get_dirent_ref(path);
    if (!parent_dirent_ref.found) {
        return -2; // Parent directory not found
    }
    if (!(parent_dirent_ref.dirent.attributes & DIRENT_DIRECTORY)) {
        return -3; // Parent is not a directory
    }
    // Find end of dirents in parent directory
    uint32_t cluster = parent_dirent_ref.cluster;
    while (1) {
        uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
        uint8_t cluster_buffer[bytes_per_cluster];
        uint32_t first_sector = get_first_cluster_sector(cluster);
        read_sectors_relative(active_disk, active_partition, first_sector, cluster_buffer, bpb.sectors_per_cluster);

        // Search for free entry
        uint32_t offset = 0;
        while (offset < bytes_per_cluster) {
            dirent_t* current_dirent = (dirent_t*)&cluster_buffer[offset];
            if (current_dirent->name[0] == DIRENT_END || current_dirent->name[0] == DIRENT_DELETED) {
                // Found free entry
                memcpy(current_dirent, &dirent, sizeof(dirent_t));
                // Write back the cluster
                write_sectors_relative(active_disk, active_partition, first_sector, cluster_buffer, bpb.sectors_per_cluster);
                return 0; // Success
            }
            offset += sizeof(dirent_t);
        }
        // Move to next cluster
        uint32_t next = next_cluster(cluster);
        if (next >= CLUSTER_CHAIN_END) {
            // No more clusters, need to allocate a new one
            uint32_t free_cluster = fat_compute_free_cluster();
            if (free_cluster == 0) {
                return -4; // No free clusters available
            }
            // Update FAT to link new cluster
            write_fat(cluster, free_cluster | 0x0FFFFFFF);
            write_fat(free_cluster, CLUSTER_CHAIN_END);
            // Clear new cluster
            uint32_t new_first_sector = get_first_cluster_sector(free_cluster);
            uint8_t zero_buffer[bpb.sectors_per_cluster * bpb.bytes_per_sector];
            memset(zero_buffer, 0, sizeof(zero_buffer));
            write_sectors_relative(active_disk, active_partition, new_first_sector, zero_buffer, bpb.sectors_per_cluster);
            next = free_cluster;
        }
        cluster = next;
    }
}

void separate_dirname_filename(const char *path, char *dirname, char *filename) {
    int len = 0;
    while (path[len] != '\0') len++;
    int i = len - 1;
    while (i >= 0 && path[i] != '/') i--;
    if (i < 0) {
        // No directory part
        dirname[0] = '\0';
        for (int j = 0; j < 12 && path[j] != '\0'; j++) {
            filename[j] = path[j];
        }
    } else {
        // Copy dirname
        int j;
        for (j = 0; j <= i; j++) {
            dirname[j] = path[j];
        }
        dirname[j] = '\0';
        // Copy filename
        for (j = 0; j < 12 && path[i + 1 + j] != '\0'; j++) {
            filename[j] = path[i + 1 + j];
        }
    }
}

int fat_create_file(const char *path) {
    char npath[512];
    normalize_fat_path(path, npath);
    path = npath; // Replace path with normalized for the rest of the function
    if (read_only) {
        return -1; // Filesystem is read-only
    }
    if (fat_exists(path)) {
        return -1; // File already exists
    }
    dirent_t dirent;
    memset(&dirent, 0, sizeof(dirent_t));
    dirent.attributes = 0; // Regular file
    dirent.first_cluster_low = 0;
    dirent.first_cluster_high = 0;
    dirent.file_size = 0;
    uint32_t year, month, day, hour, minute, second;
    get_wall_clock_time(&year, &month, &day, &hour, &minute, &second);
    uint16_t fat_date, fat_time;
    wall_clock_to_fat_timestamp(year, month, day, hour, minute, second, &fat_date, &fat_time);
    dirent.last_modification_date = fat_date;
    dirent.last_modification_time = fat_time;
    dirent.creation_date = fat_date;
    dirent.creation_time = fat_time;

    // Extract filename from path
    char dirname[256];
    char filename[12];
    separate_dirname_filename(path, dirname, filename);
    readable_to_8d3(filename);
    memcpy(dirent.name, filename, 11);

    return fat_add_dirent(dirname, dirent);
}

int fat_create_directory(const char *path) {
    char npath[512];
    normalize_fat_path(path, npath);
    path = npath; // Replace path with normalized for the rest of the function
    if (read_only) {
        return -1; // Filesystem is read-only
    }
    if (fat_exists(path)) {
        return -1; // Directory already exists
    }
    dirent_t dirent;
    memset(&dirent, 0, sizeof(dirent_t));
    dirent.attributes = DIRENT_DIRECTORY;
    uint32_t year, month, day, hour, minute, second;
    get_wall_clock_time(&year, &month, &day, &hour, &minute, &second);
    uint16_t fat_date, fat_time;
    wall_clock_to_fat_timestamp(year, month, day, hour, minute, second, &fat_date, &fat_time);
    dirent.last_modification_date = fat_date;
    dirent.last_modification_time = fat_time;
    dirent.creation_date = fat_date;
    dirent.creation_time = fat_time;

    // Allocate new cluster
    uint32_t free_cluster = fat_compute_free_cluster();
    if (free_cluster == 0) {
        return -4; // No free clusters available
    }
    // Update FAT to mark end of chain
    write_fat(free_cluster, CLUSTER_CHAIN_END);
    // Clear new cluster
    uint32_t new_first_sector = get_first_cluster_sector(free_cluster);
    uint8_t zero_buffer[bpb.sectors_per_cluster * bpb.bytes_per_sector];
    memset(zero_buffer, 0, sizeof(zero_buffer));
    write_sectors_relative(active_disk, active_partition, new_first_sector, zero_buffer, bpb.sectors_per_cluster);
    dirent.first_cluster_low  = free_cluster & 0xFFFF;
    dirent.first_cluster_high = free_cluster >> 16;

    // Extract filename from path
    char dirname[256];
    char filename[12];
    separate_dirname_filename(path, dirname, filename);
    readable_to_8d3(filename);

    memcpy(dirent.name, filename, 11);

    int res = fat_add_dirent(dirname, dirent);
    if (res != 0) {
        return -5; // Failed to add dirent
    }
    // Initialize the new directory cluster with '.' and '..' entries
    uint8_t sector_buffer[512];
    dirent_t* entries = (dirent_t*)sector_buffer;
    memcpy(entries[0].name, ".          ", 11);
    entries[0].attributes = DIRENT_DIRECTORY;
    entries[0].first_cluster_low = free_cluster & 0xFFFF;
    entries[0].first_cluster_high = (free_cluster >> 16) & 0xFFFF;
    entries[0].last_modification_date = fat_date;
    entries[0].last_modification_time = fat_time;
    entries[0].creation_date = fat_date;
    entries[0].creation_time = fat_time;
    dirent_ref_t parent = fat_get_dirent_ref(dirname);
    memcpy(entries[1].name, "..         ", 11);
    entries[1].attributes = DIRENT_DIRECTORY;
    entries[1].first_cluster_low = parent.dirent.first_cluster_low;
    entries[1].first_cluster_high = parent.dirent.first_cluster_high;
    entries[1].last_modification_date = fat_date;
    entries[1].last_modification_time = fat_time;
    entries[1].creation_date = fat_date;
    entries[1].creation_time = fat_time;
    entries[2].name[0] = DIRENT_END; // End entry
    uint32_t first_sector = get_first_cluster_sector(free_cluster);
    res = write_sectors_relative(active_disk, active_partition, first_sector, sector_buffer, 1);
    if (res != 0) {
        return -6; // Failed to add '.' or '..' entries
    }
    return 0;
}

int fat_write_to_file(const char *path, const uint8_t *buffer, size_t offset, size_t size) {
    if (read_only) {
        return -1; // Filesystem is read-only
    }
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found) {
        return -2; // File not found
    }
    uint32_t file_size = dirent_ref.dirent.file_size;

    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t current_cluster = dirent_ref.cluster;
    uint32_t cluster_offset = 0;
    size_t bytes_written = 0;

    // Handle new files
    if (dirent_ref.dirent.first_cluster_low == 0) {
        uint32_t free_cluster = fat_compute_free_cluster();
        dirent_ref.dirent.first_cluster_low = free_cluster & 0xFFFF;
        dirent_ref.dirent.first_cluster_high = (free_cluster >> 16) & 0xFFFF;
        write_fat(free_cluster, CLUSTER_CHAIN_END);
        current_cluster = free_cluster;
    }

    // Skip clusters to reach the offset
    while (offset >= cluster_size) {
        if (current_cluster >= CLUSTER_CHAIN_END) {
            // Allocate new cluster
            uint32_t free_cluster = fat_compute_free_cluster();
            if (free_cluster == 0) {
                return -3; // No free clusters available
            }
            // Update FAT to link new cluster
            write_fat(current_cluster, free_cluster | 0x0FFFFFFF);
            write_fat(free_cluster, CLUSTER_CHAIN_END); // Mark end of chain
        } else {
            current_cluster = next_cluster(current_cluster);
        }
        offset -= cluster_size;
    }
    cluster_offset = offset;

    // Write data
    while (bytes_written < size) {
        if (current_cluster >= CLUSTER_CHAIN_END) {
            // Allocate new cluster
            uint32_t free_cluster = fat_compute_free_cluster();
            if (free_cluster == 0) {
                return -4; // No free clusters available
            }
            // Update FAT to link new cluster
            write_fat(current_cluster, free_cluster | 0x0FFFFFFF);
            write_fat(free_cluster, CLUSTER_CHAIN_END); // Mark end of chain
        }
        uint32_t first_sector = get_first_cluster_sector(current_cluster);
        uint8_t cluster_buffer[cluster_size];
        read_sectors_relative(active_disk, active_partition, first_sector, cluster_buffer, bpb.sectors_per_cluster);

        while (cluster_offset < cluster_size && bytes_written < size) {
            cluster_buffer[cluster_offset++] = buffer[bytes_written++];
        }

        // Write back the modified cluster
        write_sectors_relative(active_disk, active_partition, first_sector, cluster_buffer, bpb.sectors_per_cluster);

        if (bytes_written < size) {
            current_cluster = next_cluster(current_cluster);
            cluster_offset = 0;
        }
    }
    // Update file size if needed
    if (offset + size > file_size) {
        dirent_ref.dirent.file_size = offset + size;
    }
    // Update last modification time
    uint32_t year, month, day, hour, minute, second;
    get_wall_clock_time(&year, &month, &day, &hour, &minute, &second);
    uint16_t fat_date, fat_time;
    wall_clock_to_fat_timestamp(year, month, day, hour, minute, second, &fat_date, &fat_time);
    dirent_ref.dirent.last_modification_date = fat_date;
    dirent_ref.dirent.last_modification_time = fat_time;
    // Write back updated dirent
    uint8_t sector_buffer[512];
    read_sectors_relative(active_disk, active_partition, dirent_ref.position[0], sector_buffer, 1);
    memcpy(&sector_buffer[dirent_ref.position[1]], &dirent_ref.dirent, sizeof(dirent_t));
    write_sectors_relative(active_disk, active_partition, dirent_ref.position[0], sector_buffer, 1);
    return bytes_written;
}

int fat_get_creation_time(const char *path, uint64_t *timestamp) {
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found) {
        *timestamp = 0;
        return -1; // File not found
    }
    uint16_t fat_date = dirent_ref.dirent.creation_date;
    uint16_t fat_time = dirent_ref.dirent.creation_time;
    *timestamp = fat_timestamp_to_unix(fat_date, fat_time);
    return 0;
}

int fat_get_last_modification_time(const char *path, uint64_t *timestamp) {
    dirent_ref_t dirent_ref = fat_get_dirent_ref(path);
    if (!dirent_ref.found) {
        *timestamp = 0;
        return -1; // File not found
    }
    uint16_t fat_date = dirent_ref.dirent.last_modification_date;
    uint16_t fat_time = dirent_ref.dirent.last_modification_time;
    *timestamp = fat_timestamp_to_unix(fat_date, fat_time);
    return 0;
}

void fat_register() {
    filesystem_t fat_fs;
    memset(&fat_fs, 0, sizeof(filesystem_t));
    memcpy(fat_fs.name, "FAT", 6);
    fat_fs.check = is_fat_partition;
    fat_fs.select = fat_init;
    fat_fs.set_read_only = fat_set_read_only;
    fat_fs.exists = fat_exists;
    fat_fs.is_directory = fat_is_directory;
    fat_fs.get_file_size = fat_get_file_size;
    fat_fs.read = fat_read;
    fat_fs.write = fat_write_to_file;
    fat_fs.list = fat_list;
    fat_fs.create_file = fat_create_file;
    fat_fs.create_directory = fat_create_directory;
    fat_fs.remove = fat_delete;
    fat_fs.get_creation_time = fat_get_creation_time;
    fat_fs.get_last_modification_time = fat_get_last_modification_time;

    register_filesystem(fat_fs);
}
