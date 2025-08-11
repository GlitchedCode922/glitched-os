#include "fat32.h"
#include "../drivers/partitions/mbr.h"
#include "../memory/mman.h"
#include <stddef.h>
#include <stdint.h>

uint8_t active_disk;
uint8_t active_partition;
bpb_t bpb;
fsinfo_t fsinfo;
uint8_t read_only = 0;

void get_wall_clock_time(uint32_t* year, uint32_t* month, uint32_t* day,
                         uint32_t* hour, uint32_t* minute, uint32_t* second) {
    // This function is a placeholder. It will be added with the RTC driver in timer.c.
}

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

void set_read_only(uint8_t read_only_flag) {
    read_only = read_only_flag;
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

    uint8_t sector[512];
    read_sectors_relative(active_disk, active_partition, fat_sector, sector, 1);
    uint32_t entry = *(uint32_t*)&sector[fat_entry_offset];
    uint32_t next = entry & 0x0FFFFFFF;
    return next;
}

uint32_t get_cluster_size() {
    return bpb.bytes_per_sector * bpb.sectors_per_cluster;
}

uint32_t get_cluster_start(uint32_t cluster) {
    uint32_t first_data_sector = bpb.reserved_sectors + (bpb.num_fats * get_fat_size());
    return first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
}

static void copy_and_to_upper(const char* src, char* dest, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        char c = src[i];
        if (c >= 'a' && c <= 'z') {
            c -= 32; // Convert to uppercase
        }
        dest[i] = c;
        i++;
    }
    dest[i] = '\0'; // Null-terminate the destination string
}

int lsdir(const char* path, char* element, uint64_t element_index) {
    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

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
    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    char* path_copy = (char*)path;
    size_t path_length = 0;
    while (*path_copy) {
        path_copy++;
        path_length++;
    }
    char directory[path_length];
    char filename[path_length];

    memset(directory, 0, path_length);
    memset(filename, 0 , path_length);

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
        lsdir(directory, element, i);
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

int is_directory(const char* path) {
    char element[13] = {0};
    return lsdir(path, element, 1) == 0;
}

uint64_t get_file_size(const char *path) {
    if (is_directory(path)) return 0;

    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    uint32_t cluster = bpb.root_cluster;
    char subdir[12] = {0}; // 8.3 name + null terminator
    int path_pos = 0;

    dirent_t dirent_copy;

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

                    if (memcmp(dirent->name, subdir, 11) == 0) {
                        // Found subdir; move to its cluster
                        cluster = ((uint32_t)dirent->first_cluster_high << 16) | dirent->first_cluster_low;
                        found = 1;
                        // Copy the dirent for later use
                        dirent_copy = *dirent;
                        break;
                    }
                }
            }

            if (!found) {
                cluster = get_next_cluster(cluster);
            }
        }

        if (!found) {
            return 0; // Subdirectory not found
        }
    }

    // Step 3: Return file size
    return dirent_copy.file_size;
}

int read_from_file(const char* path, uint8_t* buffer, size_t offset, size_t size) {
    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    // Step 1: Separate directory and filename
    int path_len = 0;
    while (path[path_len] != '\0') path_len++;

    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash == -1 || last_slash == path_len - 1) {
        return -1; // Invalid path
    }

    char dir_path[256] = {0};
    char file_name[12] = {0}; // 8.3 padded name (no dot), 11 chars + null

    // Copy directory path
    for (int i = 0; i < last_slash && i < sizeof(dir_path) - 1; i++) {
        dir_path[i] = path[i];
    }

    // Prepare file name (strip dot if present)
    int name_index = 0;
    int i = last_slash + 1;
    while (i < path_len && name_index < 11) {
        if (path[i] == '.') {
            while (name_index < 8) file_name[name_index++] = ' '; // pad name
            i++;
            continue;
        }
        file_name[name_index++] = path[i++];
    }
    while (name_index < 11) file_name[name_index++] = ' ';

    // Step 2: Traverse to directory cluster
    uint32_t cluster = bpb.root_cluster;
    if (dir_path[0] != '\0') {
        char subdir[12] = {0}; // 8.3 name + null terminator
        int path_pos = 0;
        int dir_path_len = 0;

        // Get the length of the directory path
        while (dir_path[dir_path_len] != '\0') dir_path_len++;

        // Walk through the directory path
        while (path_pos < dir_path_len) {
            int subdir_len = 0;

            // Extract the next subdirectory name
            while (dir_path[path_pos] != '/' && path_pos < dir_path_len && subdir_len < 11) {
                subdir[subdir_len++] = dir_path[path_pos++];
            }
            while (dir_path[path_pos] == '/') path_pos++; // Skip consecutive slashes
            for (int i = subdir_len; i < 11; i++) subdir[i] = ' '; // Pad with spaces

            // Search for the subdirectory in the current cluster
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
                            // Found the subdirectory; update the cluster
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
                return -2; // Subdirectory not found
            }
        }
    }

    // Step 3: Find the file dirent
    uint32_t file_cluster = 0;
    dirent_t file_dirent = {0};
    int file_found = 0;

    while (cluster < 0x0FFFFFF8) {
        for (uint32_t sector = 0; sector < bpb.sectors_per_cluster; sector++) {
            uint8_t sector_buffer[512];
            read_sectors_relative(active_disk, active_partition,
                get_cluster_start(cluster) + sector, sector_buffer, 1);

            dirent_t* dirent = (dirent_t*)sector_buffer;
            for (int entry = 0; entry < 512 / sizeof(dirent_t); entry++, dirent++) {
                if (dirent->name[0] == 0x00) break; // End of entries
                if (dirent->name[0] == 0xE5 || dirent->attributes & DIRENT_VOLUME_LABEL) continue;

                if (memcmp(dirent->name, file_name, 11) == 0 && !(dirent->attributes & DIRENT_DIRECTORY)) {
                    file_cluster = ((uint32_t)dirent->first_cluster_high << 16) | dirent->first_cluster_low;
                    file_dirent = *dirent;
                    file_found = 1;
                    break;
                }
            }
            if (file_found) break;
        }
        if (file_found) break;
        cluster = get_next_cluster(cluster);
    }

    if (!file_found || file_cluster >= 0x0FFFFFF8) {
        return -3; // File not found
    }

    // Step 4: Read data from the file
    uint32_t file_size = file_dirent.file_size;
    if (offset >= file_size) return -4;

    if (offset + size > file_size) size = file_size - offset;

    uint32_t bytes_read = 0;
    uint32_t cluster_size = get_cluster_size();
    uint32_t current_cluster = file_cluster;
    uint32_t current_offset = offset;

    // Skip clusters until offset is reached
    while (current_offset >= cluster_size) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= 0x0FFFFFF8) return -5;
        current_offset -= cluster_size;
    }

    uint8_t sector_buffer[512];
    while (bytes_read < size) {
        uint32_t cluster_start_sector = get_cluster_start(current_cluster);
        uint32_t sector_in_cluster = current_offset / bpb.bytes_per_sector;
        uint32_t byte_offset_in_sector = current_offset % bpb.bytes_per_sector;

        read_sectors_relative(active_disk, active_partition,
            cluster_start_sector + sector_in_cluster, sector_buffer, 1);

        uint32_t bytes_to_copy = bpb.bytes_per_sector - byte_offset_in_sector;
        if (bytes_to_copy > size - bytes_read)
            bytes_to_copy = size - bytes_read;

        for (uint32_t i = 0; i < bytes_to_copy; i++) {
            buffer[bytes_read + i] = sector_buffer[byte_offset_in_sector + i];
        }

        bytes_read += bytes_to_copy;
        current_offset += bytes_to_copy;

        if (current_offset >= cluster_size) {
            current_cluster = get_next_cluster(current_cluster);
            if (current_cluster >= 0x0FFFFFF8) break;
            current_offset = 0;
        }
    }

    return bytes_read;
}

uint32_t update_free_cluster() {
    if (read_only) return 1; // Cannot update free cluster in read-only mode
    fsinfo.free_clusters--;
    int query_cluster = fsinfo.next_free_cluster;
    int clusters_existing = get_fat_size() * bpb.bytes_per_sector / 4; // Number of clusters in FAT
    while (query_cluster < clusters_existing) {
        if (get_next_cluster(query_cluster) == CLUSTER_FREE) {
            fsinfo.next_free_cluster = query_cluster;
            write_sectors_relative(active_disk, active_partition, bpb.fs_info * (bpb.bytes_per_sector / 512), (uint8_t*)&fsinfo, 1);
            return query_cluster;
        }
        query_cluster++;
    }
    fsinfo.next_free_cluster = 0; // Reset to 0 if no free cluster found
    fsinfo.free_clusters++;
    write_sectors_relative(active_disk, active_partition, bpb.fs_info * (bpb.bytes_per_sector / 512), (uint8_t*)&fsinfo, 1);
    return 0; // No free cluster found
}

uint32_t get_free_cluster() {
    return update_free_cluster();
}

uint32_t add_to_chain(uint32_t previous_eoc) {
    if (read_only) return 1; // Cannot add to chain in read-only mode
    uint32_t new_cluster = get_free_cluster();
    if (new_cluster == 0) {
        return 0; // No free cluster available
    }

    // Mark the new cluster as used in the FAT
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_offset = previous_eoc * 4; // Each FAT entry is 4 bytes
    uint32_t fat_sector = fat_start + (fat_offset / bpb.bytes_per_sector);
    uint32_t fat_entry_offset = fat_offset % bpb.bytes_per_sector;

    uint8_t sector[512];
    if (previous_eoc < 0x0FFFFFF8) { // Check if previous_eoc is a valid cluster, if not, create a new chain
        read_sectors_relative(active_disk, active_partition, fat_sector, sector, 1);
        *(uint32_t*)&sector[fat_entry_offset] = new_cluster & 0x0FFFFFFF; // Set next cluster
        write_sectors_relative(active_disk, active_partition, fat_sector, sector, 1);
    }

    fat_offset = new_cluster * 4; // Update offset for the new cluster
    fat_sector = fat_start + (fat_offset / bpb.bytes_per_sector);
    fat_entry_offset = fat_offset % bpb.bytes_per_sector;

    read_sectors_relative(active_disk, active_partition, fat_sector, sector, 1);
    *(uint32_t*)&sector[fat_entry_offset] = CLUSTER_CHAIN_END; // Mark the new cluster as EOC
    write_sectors_relative(active_disk, active_partition, fat_sector, sector, 1);

    update_free_cluster();

    return new_cluster;
}

int add_dirent(const char* path, dirent_t dirent) {
    if (read_only) return -10; // Cannot add dirent in read-only mode
    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    // Step 2: Traverse to directory cluster
    uint32_t cluster = bpb.root_cluster;
    if (path[0] != '\0') {
        char subdir[12] = {0}; // 8.3 name + null terminator
        int path_pos = 0;
        int path_len = 0;

        // Get the length of the directory path
        while (path[path_len] != '\0') path_len++;

        // Walk through the directory path
        while (path_pos < path_len) {
            int subdir_len = 0;

            // Extract the next subdirectory name
            while (path[path_pos] != '/' && path_pos < path_len && subdir_len < 11) {
                subdir[subdir_len++] = path[path_pos++];
            }
            while (path[path_pos] == '/') path_pos++; // Skip consecutive slashes
            for (int i = subdir_len; i < 11; i++) subdir[i] = ' '; // Pad with spaces

            // Search for the subdirectory in the current cluster
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
                            // Found the subdirectory; update the cluster
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
                return -2; // Subdirectory not found
            }
        }
    }

    // Step 3: Find the end of the directory chain
    uint32_t dirents_per_cluster = (bpb.sectors_per_cluster * bpb.bytes_per_sector) / sizeof(dirent_t);
    uint32_t dirents_per_sector = bpb.bytes_per_sector / sizeof(dirent_t);
    uint32_t current_cluster = cluster;
    uint32_t current_dirent_index = 0;
    uint32_t last_cluster = 0;
    uint32_t last_dirent_index = 0;

    while (current_cluster < 0x0FFFFFF8) {
        for (uint32_t sector = 0; sector < bpb.sectors_per_cluster; sector++) {
            uint8_t sector_buffer[512];
            read_sectors_relative(active_disk, active_partition,
                get_cluster_start(current_cluster) + sector, sector_buffer, 1);

            dirent_t* dirent_ptr = (dirent_t*)sector_buffer;
            for (int entry = 0; entry < dirents_per_sector; entry++, dirent_ptr++) {
                if (dirent_ptr->name[0] == 0x00) {
                    // Empty entry found, use this one
                    memcpy(dirent_ptr, &dirent, sizeof(dirent_t));

                    // Add the terminating entry
                    dirent_t end_entry; // Create an empty entry to mark the end
                    end_entry.name[0] = 0x00; // Set the first byte to 0x00 to indicate end of entries
                    memcpy(dirent_ptr + 1, &end_entry, sizeof(dirent_t));

                    write_sectors_relative(active_disk, active_partition,
                        get_cluster_start(current_cluster) + sector, sector_buffer, 1);
                    return 0; // Successfully added
                }
                if (dirent_ptr->name[0] == 0xE5 || dirent_ptr->attributes & DIRENT_VOLUME_LABEL) continue;

                current_dirent_index++;
            }
        }
        last_cluster = current_cluster;
        last_dirent_index = current_dirent_index;
        current_cluster = get_next_cluster(current_cluster);
    }
    // If we reach here, we need to add a new cluster
    uint32_t new_cluster = add_to_chain(last_cluster);
    if (new_cluster == 0) {
        return -3; // No free cluster available
    }
    // Write the new dirent to the new cluster
    uint8_t sector_buffer[512];
    read_sectors_relative(active_disk, active_partition,
        get_cluster_start(new_cluster), sector_buffer, 1);
    dirent_t* dirent_cast = (dirent_t*)sector_buffer;
    memcpy(dirent_cast, &dirent, sizeof(dirent_t));

    // Add the terminating entry
    dirent_t end_entry; // Create an empty entry to mark the end
    end_entry.name[0] = 0x00; // Set the first byte to 0x00 to indicate end of entries
    memcpy(dirent_cast + 1, &end_entry, sizeof(dirent_t));

    write_sectors_relative(active_disk, active_partition,
        get_cluster_start(new_cluster), sector_buffer, 1);

    return 0; // Successfully added to new cluster
}

int delete_entry(const char* path) {
    if (read_only) return -10; // Cannot delete entry in read-only mode
    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    // Step 1: Separate directory and filename

    // Handle leading/trailing slashes
    while (*path == '/') path++; // Skip leading slashes

    int path_len = 0;
    while (path[path_len] != '\0') path_len++;

    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    while (path_len > 0 && path[path_len - 1] == '/') path_len--; // Remove trailing slashes

    char dir_path[256] = {0};
    char file_name[12] = {0}; // 8.3 padded name (no dot), 11 chars + null

    // Copy directory path
    for (int i = 0; i < last_slash && i < sizeof(dir_path) - 1; i++) {
        dir_path[i] = path[i];
    }

    // Prepare file name (strip dot if present)
    int name_index = 0;
    int i = last_slash + 1;
    while (i < path_len && name_index < 11) {
        if (path[i] == '.') {
            while (name_index < 8) file_name[name_index++] = ' '; // pad name
            i++;
            continue;
        }
        file_name[name_index++] = path[i++];
    }
    while (name_index < 11) file_name[name_index++] = ' ';

    // Step 2: Traverse to directory cluster
    uint32_t cluster = bpb.root_cluster;
    if (dir_path[0] != '\0') {
        char subdir[12] = {0}; // 8.3 name + null terminator
        int path_pos = 0;
        int dir_path_len = 0;

        // Get the length of the directory path
        while (dir_path[dir_path_len] != '\0') dir_path_len++;

        // Walk through the directory path
        while (path_pos < dir_path_len) {
            int subdir_len = 0;

            // Extract the next subdirectory name
            while (dir_path[path_pos] != '/' && path_pos < dir_path_len && subdir_len < 11) {
                subdir[subdir_len++] = dir_path[path_pos++];
            }
            while (dir_path[path_pos] == '/') path_pos++; // Skip consecutive slashes
            for (int i = subdir_len; i < 11; i++) subdir[i] = ' '; // Pad with spaces

            // Search for the subdirectory in the current cluster
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
                            // Found the subdirectory; update the cluster
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
                return -2; // Subdirectory not found
            }
        }
    }

    // Step 3: Find the file dirent
    uint32_t file_cluster = 0;
    dirent_t file_dirent = {0};
    int file_found = 0;

    uint32_t file_dirent_position[2]; // To store the position of the file dirent

    while (cluster < 0x0FFFFFF8) {
        for (uint32_t sector = 0; sector < bpb.sectors_per_cluster; sector++) {
            uint8_t sector_buffer[512];
            read_sectors_relative(active_disk, active_partition,
                get_cluster_start(cluster) + sector, sector_buffer, 1);

            dirent_t* dirent = (dirent_t*)sector_buffer;
            for (int entry = 0; entry < 512 / sizeof(dirent_t); entry++, dirent++) {
                if (dirent->name[0] == 0x00) break; // End of entries
                if (dirent->name[0] == 0xE5 || dirent->attributes & DIRENT_VOLUME_LABEL) continue;

                if (memcmp(dirent->name, file_name, 11) == 0 && !(dirent->attributes & DIRENT_DIRECTORY)) {
                    file_cluster = ((uint32_t)dirent->first_cluster_high << 16) | dirent->first_cluster_low;
                    file_dirent = *dirent;
                    file_dirent_position[0] = get_cluster_start(cluster) + sector;
                    file_dirent_position[1] = entry * sizeof(dirent_t); // Position within the sector
                    file_found = 1;
                    break;
                }
            }
            if (file_found) break;
        }
        if (file_found) break;
        cluster = get_next_cluster(cluster);
    }

    if (!file_found || file_cluster >= 0x0FFFFFF8) {
        return -3; // File not found
    }

    // Step 4: Mark the file as deleted
    file_dirent.name[0] = 0xE5; // Mark as deleted
    uint8_t sector_buffer[512];
    read_sectors_relative(active_disk, active_partition,
        file_dirent_position[0], sector_buffer, 1);
    dirent_t* dirent_cast = (dirent_t*)(sector_buffer + file_dirent_position[1]);
    memcpy(dirent_cast, &file_dirent, sizeof(dirent_t));
    write_sectors_relative(active_disk, active_partition,
        file_dirent_position[0], sector_buffer, 1);
    return 0; // Successfully deleted
}

void create_file(const char* path) {
    if (read_only) return; // Cannot create file in read-only mode
    if (file_exists(path) || is_directory(path)) return; // File already exists or path is a directory

    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    dirent_t new_file;
    memset(&new_file, 0, sizeof(dirent_t));
    new_file.attributes = DIRENT_ARCHIVE; // Set file attribute
    
    // Set the name in 8.3 format

    // Handle leading/trailing slashes
    while (*path == '/') path++; // Skip leading slashes

    int path_len = 0;
    while (path[path_len] != '\0') path_len++;

    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    while (path_len > 0 && path[path_len - 1] == '/') path_len--; // Remove trailing slashes

    char dir_path[256] = {0};
    char file_name[12] = {0}; // 8.3 padded name (no dot), 11 chars + null

    // Copy directory path
    for (int i = 0; i < last_slash && i < sizeof(dir_path) - 1; i++) {
        dir_path[i] = path[i];
    }

    // Prepare file name (strip dot if present)
    int name_index = 0;
    int i = last_slash + 1;
    while (i < path_len && name_index < 11) {
        if (path[i] == '.') {
            while (name_index < 8) file_name[name_index++] = ' '; // pad name
            i++;
            continue;
        }
        file_name[name_index++] = path[i++];
    }
    while (name_index < 11) file_name[name_index++] = ' ';

    // Copy the file name into the dirent
    memcpy(new_file.name, file_name, 11);

    // Allocate a cluster for the file
    uint32_t free_cluster = add_to_chain(CLUSTER_CHAIN_END);

    if (free_cluster == 0) {
        return; // No free cluster available
    }

    new_file.first_cluster_high = (free_cluster >> 16) & 0xFFFF; // High part of the cluster number
    new_file.first_cluster_low = free_cluster & 0xFFFF; // Low part of the cluster number
    new_file.file_size = 0; // Initial file size is 0

    add_dirent(dir_path, new_file);
}

int write_to_file(const char* path, const uint8_t* buffer, size_t offset, size_t size) {
    if (read_only) return -10; // Cannot write to file in read-only mode
    if (size == 0) return 0; // Nothing to write
    if (!file_exists(path)) create_file(path); // Create the file if it doesn't exist
    if (!file_exists(path)) return -1; // Still not found after creation attempt
    if (is_directory(path)) return -2; // Cannot write to a directory

    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    // Create the timestamp
    uint16_t fat_time;
    uint16_t fat_date;
    uint32_t year, month, day, hour, minute, second;
    get_wall_clock_time(&year, &month, &day, &hour, &minute, &second);
    wall_clock_to_fat32_timestamp(year, month, day, hour, minute, second, &fat_time, &fat_date);

    // Step 1: Separate directory and filename

    // Handle leading/trailing slashes
    while (*path == '/') path++; // Skip leading slashes

    int path_len = 0;
    while (path[path_len] != '\0') path_len++;

    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    while (path_len > 0 && path[path_len - 1] == '/') path_len--; // Remove trailing slashes

    char dir_path[256] = {0};
    char file_name[12] = {0}; // 8.3 padded name (no dot), 11 chars + null

    // Copy directory path
    for (int i = 0; i < last_slash && i < sizeof(dir_path) - 1; i++) {
        dir_path[i] = path[i];
    }

    // Prepare file name (strip dot if present)
    int name_index = 0;
    int i = last_slash + 1;
    while (i < path_len && name_index < 11) {
        if (path[i] == '.') {
            while (name_index < 8) file_name[name_index++] = ' '; // pad name
            i++;
            continue;
        }
        file_name[name_index++] = path[i++];
    }
    while (name_index < 11) file_name[name_index++] = ' ';

    // Step 2: Traverse to directory cluster
    uint32_t cluster = bpb.root_cluster;
    if (dir_path[0] != '\0') {
        char subdir[12] = {0}; // 8.3 name + null terminator
        int path_pos = 0;
        int dir_path_len = 0;

        // Get the length of the directory path
        while (dir_path[dir_path_len] != '\0') dir_path_len++;

        // Walk through the directory path
        while (path_pos < dir_path_len) {
            int subdir_len = 0;

            // Extract the next subdirectory name
            while (dir_path[path_pos] != '/' && path_pos < dir_path_len && subdir_len < 11) {
                subdir[subdir_len++] = dir_path[path_pos++];
            }
            while (dir_path[path_pos] == '/') path_pos++; // Skip consecutive slashes
            for (int i = subdir_len; i < 11; i++) subdir[i] = ' '; // Pad with spaces

            // Search for the subdirectory in the current cluster
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
                            // Found the subdirectory; update the cluster
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
                return -2; // Subdirectory not found
            }
        }
    }

    // Step 3: Find the file dirent
    uint32_t file_cluster = 0;
    dirent_t file_dirent = {0};
    uint32_t file_dirent_position[2]; // To store the position of the file dirent
    int file_found = 0;

    while (cluster < 0x0FFFFFF8) {
        for (uint32_t sector = 0; sector < bpb.sectors_per_cluster; sector++) {
            uint8_t sector_buffer[512];
            read_sectors_relative(active_disk, active_partition,
                get_cluster_start(cluster) + sector, sector_buffer, 1);

            dirent_t* dirent = (dirent_t*)sector_buffer;
            for (int entry = 0; entry < 512 / sizeof(dirent_t); entry++, dirent++) {
                if (dirent->name[0] == 0x00) break; // End of entries
                if (dirent->name[0] == 0xE5 || dirent->attributes & DIRENT_VOLUME_LABEL) continue;

                if (memcmp(dirent->name, file_name, 11) == 0 && !(dirent->attributes & DIRENT_DIRECTORY)) {
                    file_cluster = ((uint32_t)dirent->first_cluster_high << 16) | dirent->first_cluster_low;
                    file_dirent = *dirent;
                    file_dirent_position[0] = get_cluster_start(cluster) + sector;
                    file_dirent_position[1] = entry * sizeof(dirent_t); // Position within the sector
                    file_found = 1;
                    break;
                }
            }
            if (file_found) break;
        }
        if (file_found) break;
        cluster = get_next_cluster(cluster);
    }

    if (!file_found || file_cluster >= 0x0FFFFFF8) {
        return -3; // File not found
    }

    // Step 4: Write data to the file
    uint32_t file_size = file_dirent.file_size;
    if (offset + size > file_size) {
        while (offset + size > file_size && file_size != 0) {
            // If the file size is less than the offset + size, we need to extend the file
            uint32_t new_cluster = add_to_chain(file_cluster);
            if (new_cluster == 0) {
                return -4; // No free cluster available
            }
            file_size += get_cluster_size();
        }
        file_size = offset + size; // Update file size
        file_dirent.file_size = file_size;
        uint8_t sector_buffer[512] = {0};
        read_sectors_relative(active_disk, active_partition,
            file_dirent_position[0], sector_buffer, 1);
        dirent_t* dirent_cast = (dirent_t*)(sector_buffer + file_dirent_position[1]);
        memcpy(dirent_cast, &file_dirent, sizeof(dirent_t));
        write_sectors_relative(active_disk, active_partition,
            file_dirent_position[0], sector_buffer, 1);
    }

    uint32_t bytes_written = 0;
    uint32_t cluster_size = get_cluster_size();
    uint32_t current_cluster = file_cluster;
    uint32_t current_offset = 0;
    uint8_t sector_buffer[512] = {0};
    // Skip clusters until offset is reached
    while (current_offset < (offset / cluster_size) * cluster_size) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= 0x0FFFFFF8) add_to_chain(current_cluster); // If we reach the end of chain, add a new cluster
        current_offset += cluster_size;
    }

    // Now we are at the correct cluster
    offset %= cluster_size; // Adjust offset to be within the cluster
    while (bytes_written < size) {
        uint32_t cluster_start_sector = get_cluster_start(current_cluster);
        uint32_t sector_in_cluster = (current_offset + offset) / bpb.bytes_per_sector;
        uint32_t byte_offset_in_sector = (current_offset + offset) % bpb.bytes_per_sector;

        read_sectors_relative(active_disk, active_partition,
            cluster_start_sector + sector_in_cluster, sector_buffer, 1);

        uint32_t bytes_to_copy = bpb.bytes_per_sector - byte_offset_in_sector;
        if (bytes_to_copy > size - bytes_written)
            bytes_to_copy = size - bytes_written;

        for (uint32_t i = 0; i < bytes_to_copy; i++) {
            sector_buffer[byte_offset_in_sector + i] = buffer[bytes_written + i];
        }

        write_sectors_relative(active_disk, active_partition,
            cluster_start_sector + sector_in_cluster, sector_buffer, 1);

        bytes_written += bytes_to_copy;
        current_offset += bytes_to_copy;

        if (current_offset >= cluster_size) {
            current_cluster = get_next_cluster(current_cluster);
            if (current_cluster >= 0x0FFFFFF8) {
                current_cluster = add_to_chain(current_cluster);
                if (current_cluster == 0) return -5; // No free cluster available
            }
            current_offset = 0;
        }
    }

    // Step 5: Update the file's last modification time
    file_dirent.last_modification_time = fat_time; // Set last modification time
    file_dirent.last_modification_date = fat_date; // Set last modification date
    
    return bytes_written; // Return the number of bytes written
}

void create_directory(const char* path) {
    if (read_only) return; // Cannot create directory in read-only mode
    if (file_exists(path) || is_directory(path)) return; // Directory already exists or path is a file

    char upper_path[256] = {0};
    copy_and_to_upper(path, upper_path, sizeof(upper_path));

    // Replace `path` with `upper_path` in the rest of the function
    path = upper_path;

    dirent_t new_dir;
    memset(&new_dir, 0, sizeof(dirent_t));
    new_dir.attributes = DIRENT_DIRECTORY; // Set directory attribute

    // Create the timestamp
    uint16_t fat_time;
    uint16_t fat_date;
    uint32_t year, month, day, hour, minute, second;
    get_wall_clock_time(&year, &month, &day, &hour, &minute, &second);
    wall_clock_to_fat32_timestamp(year, month, day, hour, minute, second, &fat_time, &fat_date);

    // Set the creation and modification time and date
    new_dir.creation_time = fat_time; // Set creation time
    new_dir.creation_date = fat_date; // Set creation date
    new_dir.last_modification_time = fat_time; // Set last modification time
    new_dir.last_modification_date = fat_date; // Set last modification date

    // Set the name in 8.3 format

    // Handle leading/trailing slashes
    while (*path == '/') path++; // Skip leading slashes

    int path_len = 0;
    while (path[path_len] != '\0') path_len++;

    int last_slash = -1;
    for (int i = 0; i < path_len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    while (path_len > 0 && path[path_len - 1] == '/') path_len--; // Remove trailing slashes

    char dir_path[256] = {0};
    char dir_name[12] = {0}; // 8.3 padded name, 11 chars + null

    // Copy directory path
    for (int i = 0; i < last_slash && i < sizeof(dir_path) - 1; i++) {
        dir_path[i] = path[i];
    }

    // Prepare directory name
    int name_index = 0;
    int i = last_slash + 1;
    while (i < path_len && name_index < 11) dir_name[name_index++] = path[i++];
    while (name_index < 11) dir_name[name_index++] = ' ';

    // Copy the directory name into the dirent
    memcpy(new_dir.name, dir_name, 11);

    // Allocate a cluster for the directory
    uint32_t free_cluster = add_to_chain(CLUSTER_CHAIN_END);

    if (free_cluster == 0) {
        return; // No free cluster available
    }

    new_dir.first_cluster_high = (free_cluster >> 16) & 0xFFFF; // High part of the cluster number
    new_dir.first_cluster_low = free_cluster & 0xFFFF; // Low part of the cluster number
    new_dir.file_size = 0; // Initial directory size is 0

    add_dirent(dir_path, new_dir);

    // Create the ., .. and end entries

    dirent_t current_entry;
    dirent_t parent_entry;

    memset(&current_entry, 0, sizeof(dirent_t));
    memset(&parent_entry, 0, sizeof(dirent_t));
    current_entry.attributes = DIRENT_DIRECTORY;
    parent_entry.attributes = DIRENT_DIRECTORY;
    current_entry.name[0] = '.'; // Current directory
    memset(current_entry.name + 1, ' ', 10);
    parent_entry.name[0] = '.'; // Parent directory
    parent_entry.name[1] = '.';
    memset(parent_entry.name + 2, ' ', 9); // Pad the rest with spaces
    current_entry.first_cluster_high = new_dir.first_cluster_high;
    current_entry.first_cluster_low = new_dir.first_cluster_low; // Set to the new directory's cluster

    uint32_t cluster = bpb.root_cluster;
    if (dir_path[0] != '\0') {
        char subdir[12] = {0}; // 8.3 name + null terminator
        int path_pos = 0;
        int dir_path_len = 0;

        // Get the length of the directory path
        while (dir_path[dir_path_len] != '\0') dir_path_len++;

        // Walk through the directory path
        while (path_pos < dir_path_len) {
            int subdir_len = 0;

            // Extract the next subdirectory name
            while (dir_path[path_pos] != '/' && path_pos < dir_path_len && subdir_len < 11) {
                subdir[subdir_len++] = dir_path[path_pos++];
            }
            while (dir_path[path_pos] == '/') path_pos++; // Skip consecutive slashes
            for (int i = subdir_len; i < 11; i++) subdir[i] = ' '; // Pad with spaces

            // Search for the subdirectory in the current cluster
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
                            // Found the subdirectory; update the cluster
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
                return; // Subdirectory not found
            }
        }
    }

    parent_entry.first_cluster_high = (cluster >> 16) & 0xFFFF; // High part of the cluster number
    parent_entry.first_cluster_low = cluster & 0xFFFF; // Low part of the cluster number

    uint8_t empty_cluster[bpb.sectors_per_cluster * 512];
    memset(empty_cluster, 0, sizeof(empty_cluster)); // Clear the cluster buffer

    write_sectors_relative(active_disk, active_partition, get_cluster_start(free_cluster), empty_cluster, bpb.sectors_per_cluster); // Clear the new directory cluster

    // Step 5: Write the entries to the new directory
    add_dirent(path, current_entry); // Add the current directory entry
    add_dirent(path, parent_entry); // Add the parent directory entry
}

void wall_clock_to_fat32_timestamp(int year, int month, int day, int hour, int min, int sec, uint16_t *fat_date, uint16_t *fat_time) {
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

void fat32_timestamp_to_wall_clock(uint16_t fat_date, uint16_t fat_time, int *year, int *month, int *day, int *hour, int *min, int *sec) {
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
