#include "fat32.h"
#include "../drivers/partitions/mbr.h"
#include "../memory/mman.h"
#include <stddef.h>
#include <stdint.h>

uint8_t active_disk;
uint8_t active_partition;
bpb_t bpb;
fsinfo_t fsinfo;

void select_partition(uint8_t disk, uint8_t partition) {
    if (has_mbr(disk)) {
        active_disk = disk;
        active_partition = partition;
    }
}

void init_fat32(uint8_t disk, uint8_t partition) {
    select_partition(disk, partition);
    bpb = get_bpb();
    fsinfo = get_fsinfo(bpb.fs_info);
}

bpb_t get_bpb() {
    bpb_t bpb;
    read_sectors_relative(active_disk, active_partition, 0, (uint8_t*)&bpb, 1);
    return bpb;
}

fsinfo_t get_fsinfo(uint32_t fsinfo_sector) {
    fsinfo_t fsinfo;
    read_sectors_relative(active_disk, active_partition, fsinfo_sector * (bpb.bytes_per_sector / 512), (uint8_t*)&fsinfo, 1);
    return fsinfo;
}

uint32_t get_fat_size() {
    if (bpb.fat_size_32 != 0) {
        return bpb.fat_size_32;
    } else if (bpb.fat_size_16 != 0) {
        return bpb.fat_size_16;
    } else {
        return 0; // FAT size not defined
    }
}

uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_offset = cluster * 4; // Each FAT entry is 4 bytes
    uint32_t fat_sector = fat_start + (fat_offset / bpb.bytes_per_sector);
    uint32_t fat_entry_offset = fat_offset % bpb.bytes_per_sector;

    uint32_t next_cluster;
    read_sectors_relative(active_disk, active_partition, fat_sector, (uint8_t*)&next_cluster, 1);
    next_cluster >>= (fat_entry_offset * 8); // Adjust for the offset in the sector

    return next_cluster & 0x0FFFFFFF; // Mask to get the cluster number
}

uint32_t get_cluster_size() {
    return bpb.bytes_per_sector * bpb.sectors_per_cluster;
}

uint32_t get_cluster_start(uint32_t cluster) {
    uint32_t first_data_sector = bpb.reserved_sectors + (bpb.num_fats * get_fat_size());
    return first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
}

int lsdir(const char* path, char* element, uint64_t element_index) {
    uint32_t cluster = bpb.root_cluster;
    char subdir[12] = {0};  // 8.3 name + null terminator
    int path_pos = 0;

    // Step 1: Trim trailing slashes
    int path_len = 0;
    while (path[path_len] != '\0') path_len++; // Get path length
    while (path_len > 0 && path[path_len - 1] == '/') path_len--; // Remove trailing slashes

    // Step 2: Walk through the path
    while (path[path_pos] == '/') path_pos++; // Skip leading '/'
    while (path_pos < path_len) {
        int subdir_len = 0;

        // Extract next path component
        while (path[path_pos] != '/' && path_pos < path_len && subdir_len < 11) {
            subdir[subdir_len++] = path[path_pos++];
        }
        while (path[path_pos] == '/') path_pos++; // Skip consecutive slashes
        for (int i = subdir_len; i < 11; i++) subdir[i] = ' '; // Pad with spaces

        // Search for matching subdir in the current cluster
        int found = 0;
        while (!found && cluster < 0x0FFFFFF8) {
            for (uint32_t sector = 0; sector < bpb.sectors_per_cluster; sector++) {
                uint8_t buffer[512];
                read_sectors_relative(active_disk, active_partition,
                    get_cluster_start(cluster) + sector, buffer, 1);

                dirent_t* dirent = (dirent_t*)buffer;
                for (int entry = 0; entry < 512 / sizeof(dirent_t); entry++, dirent++) {
                    if (dirent->name[0] == 0x00) break; // End of entries
                    if (dirent->name[0] == 0xE5 || dirent->attributes & DIRENT_VOLUME_LABEL) continue;

                    if (memcmp(dirent->name, subdir, 11) == 0 &&
                        (dirent->attributes & DIRENT_DIRECTORY)) {
                        // Found subdir; move to its cluster
                        cluster = ((uint32_t)dirent->first_cluster_high << 16) | dirent->first_cluster_low;
                        found = 1;
                        break;
                    }
                }
            }

            if (!found) {
                cluster = get_next_cluster(cluster);
            }
        }

        if (!found) {
            element[0] = '\0'; // Subdirectory not found
            return -1;
        }

        // Clear subdir for next iteration
        for (int i = 0; i < 12; i++) subdir[i] = 0;
    }

    // Step 3: We are now in the target directory
    uint64_t current_index = 0;

    while (cluster < 0x0FFFFFF8) {
        for (uint32_t sector = 0; sector < bpb.sectors_per_cluster; sector++) {
            uint8_t buffer[512];
            read_sectors_relative(active_disk, active_partition,
                get_cluster_start(cluster) + sector, buffer, 1);

            dirent_t* dirent = (dirent_t*)buffer;
            for (int entry = 0; entry < 512 / sizeof(dirent_t); entry++, dirent++) {
                if (dirent->name[0] == 0x00) {
                    element[0] = '\0'; // End of entries
                    return -1;
                }
                if (dirent->name[0] == 0xE5 || dirent->attributes & DIRENT_VOLUME_LABEL) continue;

                if (current_index == element_index) {
                    // Convert 8.3 name into null-terminated string
                    int k = 0;
                    for (int i = 0; i < 8 && dirent->name[i] != ' '; i++) {
                        element[k++] = dirent->name[i];
                    }
                    if (dirent->name[8] != ' ') {
                        element[k++] = '.';
                        for (int i = 0; i < 3 && dirent->name[8 + i] != ' '; i++) {
                            element[k++] = dirent->name[8 + i];
                        }
                    }
                    element[k] = '\0';
                    return 0;
                }
                current_index++;
            }
        }
        cluster = get_next_cluster(cluster);
    }

    element[0] = '\0'; // No entry at given index
    return -2;
}

int file_exists(const char* path) {
    char* path_copy = (char*)path;
    size_t path_length = 0;
    while (*path_copy) {
        path_copy++;
        path_length++;
    }
    char directory[path_length];
    char filename[path_length];
    int filename_length = 0;
    int slashes = 0;
    path_copy = (char*)path;
    for (int i = 0; i < path_length; i++) {
        if (path[i] == '/') {
            slashes++;
        }
    }
    for (int i = 0; i < path_length && slashes; i++) {
        if (path[i] == '/') {
            slashes--;
        }
        directory[i] = path[i];
        path_copy++;
    }

    int i = 0;
    while (*path_copy) {
        filename[i++] = *path_copy++;
    }
    filename[i] = '\0';

    filename_length = i;

    char element[13] = {0}; // 8.3 name + null terminator
    i = 0;
    do {
        lsdir(path, element, i);
        // Remove trailing spaces from element
        for (int j = 11; j >= 0 && element[j] == ' '; j--) {
            element[j] = '\0';
        }
        if (memcmp(element, filename, filename_length) == 0) {
            return 1; // File exists
        }
        i++;
    } while (*element);
    return 0; // File does not exist    
}
