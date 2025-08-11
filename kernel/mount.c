#include "mount.h"
#include "memory/mman.h"
#include "fs/fat32.h"
#include <stdint.h>

filesystem_t filesystems[24] = {0};
mountpoint_t mountpoints[48] = {0};

int filesystem_count = 0;

int register_filesystem(filesystem_t fs) {
    if (filesystem_count >= 24) {
        return -1; // Maximum number of filesystems reached
    }
    filesystems[filesystem_count++] = fs;
    return 0; // Success
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++)); // copy until '\0'
    return ret;
}

static int matching_chars(const char *s1, const char *s2) {
    int count = 0;
    while (*s1 && *s2 && (*s1 == *s2)) {
        count++;
        s1++;
        s2++;
    }
    return count;
}

static int find_filesystem(const char *type) {
    for (int i = 0; i < filesystem_count; i++) {
        if (strcmp(filesystems[i].name, type) == 0) {
            return i; // Return index of the filesystem
        }
    }
    return -1; // Filesystem not found
}

static void resolve_dot_or_dotdot(const char *path, char *resolved_path) {
    char path_tree[50][256] = {0};
    int path_index = 0;
    const char *p = path;

    // Track absolute paths
    int is_absolute = (*p == '/');

    while (*p) {
        if (*p == '/') {
            p++;
            continue;
        }
        char *segment = path_tree[path_index++];
        while (*p && *p != '/') {
            *segment++ = *p++;
        }
        *segment = '\0';
    }

    // Resolve '.' and '..'
    for (int i = 0; i < path_index; i++) {
        if (strcmp(path_tree[i], ".") == 0) {
            path_tree[i][0] = 0xFF;
        } else if (strcmp(path_tree[i], "..") == 0) {
            path_tree[i][0] = 0xFF;
            if (i > 0) path_tree[i - 1][0] = 0xFF;
        }
    }

    // Rebuild
    int resolved_index = 0;
    if (is_absolute) {
        resolved_path[resolved_index++] = '/';
    }
    for (int i = 0; i < path_index; i++) {
        if (path_tree[i][0] != 0xFF) {
            if (resolved_index > (is_absolute ? 1 : 0)) {
                resolved_path[resolved_index++] = '/';
            }
            int j = 0;
            while (path_tree[i][j] != '\0') {
                resolved_path[resolved_index++] = path_tree[i][j++];
            }
        }
    }
    resolved_path[resolved_index] = '\0';
}

static void separate_mount_point_and_path(const char *path, char *mount_point, char *relative_path) {
    uint64_t mountpoints_match[48] = {0};
    for (int i = 0; i < 48; i++) {
        if (mountpoints[i].type[0] == '\0') continue;
        if (strcmp(mountpoints[i].mount_point, "/") == 0) {
            mountpoints_match[i] = 1; // root always matches at least '/'
        } else {
            mountpoints_match[i] = matching_chars(path, mountpoints[i].mount_point);
        }
    }

    int max_match = 0;
    int best_index = -1;
    for (int i = 0; i < 48; i++) {
        if (mountpoints_match[i] > max_match) {
            max_match = mountpoints_match[i];
            best_index = i;
        }
    }
    if (best_index < 0) {
        mount_point[0] = '\0';
        relative_path[0] = '\0';
        return;
    }

    strcpy(mount_point, mountpoints[best_index].mount_point);

    // Strip the mount point from the path
    if (strcmp(mount_point, "/") == 0) {
        strcpy(relative_path, path[0] == '/' ? path + 1 : path);
    } else {
        strcpy(relative_path, path + max_match);
    }
}

int mount_filesystem(const char *path, const char *type, int drive, int partition, int flags) {
    if (filesystem_count == 0) {
        return -1; // No filesystems registered
    }

    int fs_index = find_filesystem(type);
    if (fs_index < 0) {
        return -2; // Filesystem type not found
    }

    filesystem_t *fs = &filesystems[fs_index];

    // Check if the filesystem is valid
    if (fs->check && fs->check(drive, partition) != 0) {
        return -2; // Filesystem check failed
    }

    // Resolve the path to handle '.' and '..'
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);

    // Check if the filesystem is already mounted
    for (int i = 0; i < 48; i++) {
        if (mountpoints[i].type[0] && mountpoints[i].drive == drive && mountpoints[i].partition == partition) {
            return -3; // Already mounted
        }
    }

    // Find an empty mountpoint slot
    for (int i = 0; i < 48; i++) {
        if (mountpoints[i].mount_point[0] == 0) { // Unused slot
            mountpoints[i].drive = drive;
            mountpoints[i].partition = partition;
            memcpy(mountpoints[i].mount_point, resolved_path, 256);
            memcpy(mountpoints[i].type, type, 32);
            mountpoints[i].flags = flags;
            return 0; // Success
        }
    }

    return -4; // No available mountpoint slots
}

int unmount_filesystem(const char *path) {
    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, path) == 0) {
            mountpoints[i].mount_point[0] = '\0'; // Clear the mount point
            return 0; // Success
        }
    }
    return -1; // Mount point not found
}

int unmount_all_filesystems() {
    for (int i = 0; i < 48; i++) {
        mountpoints[i].mount_point[0] = '\0'; // Clear all mount points
    }
    return 0; // Success
}

int list_directory(const char *path, char *element, uint64_t element_index) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->list) {
                return fs->list(relative_path, element, element_index);
            }
            return -2; // Filesystem does not support listing
        }
    }
    return -1; // Mount point not found
}

int exists(const char *path) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->exists) {
                return fs->exists(relative_path);
            }
            return -2; // Filesystem does not support existence check
        }
    }
    return -1; // Mount point not found
}

int is_directory(const char *path) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->is_directory) {
                return fs->is_directory(relative_path);
            }
            return -2; // Filesystem does not support directory check
        }
    }
    return -1; // Mount point not found
}

uint64_t get_file_size(const char *path) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->get_file_size) {
                return fs->get_file_size(relative_path);
            }
            return 0; // Filesystem does not support file size retrieval
        }
    }
    return 0; // Mount point not found
}

int read_file(const char *path, uint8_t *buffer, size_t offset, size_t size) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->read) {
                return fs->read(relative_path, buffer, offset, size);
            }
            return -2; // Filesystem does not support reading
        }
    }
    return -1; // Mount point not found
}

int write_file(const char *path, const uint8_t *buffer, size_t offset, size_t size) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            if (mountpoints[i].flags & FLAG_READ_ONLY) {
                return -3; // Cannot write to read-only filesystem
            }
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->write) {
                return fs->write(relative_path, buffer, offset, size);
            }
            return -2; // Filesystem does not support writing
        }
    }
    return -1; // Mount point not found
}

int remove_file(const char *path) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            if (mountpoints[i].flags & FLAG_READ_ONLY) {
                return -3; // Cannot remove file in read-only mode
            }
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->remove) {
                return fs->remove(relative_path);
            }
            return -2; // Filesystem does not support removal
        }
    }
    return -1; // Mount point not found
}

int create_file(const char *path) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            if (mountpoints[i].flags & FLAG_READ_ONLY) {
                return -3; // Cannot create file in read-only mode
            }
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->create_file) {
                fs->create_file(relative_path);
                return 0; // Success
            }
            return -2; // Filesystem does not support file creation
        }
    }
    return -1; // Mount point not found
}

int create_directory(const char *path) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            if (mountpoints[i].flags & FLAG_READ_ONLY) {
                return -3; // Cannot create directory in read-only mode
            }
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->create_directory) {
                fs->create_directory(relative_path);
                return 0; // Success
            }
            return -2; // Filesystem does not support directory creation
        }
    }
    return -1; // Mount point not found
}

int get_creation_time(const char *path, uint64_t *timestamp) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->get_creation_time) {
                return fs->get_creation_time(relative_path, timestamp);
            }
            return -2; // Filesystem does not support creation time retrieval
        }
    }
    return -1; // Mount point not found
}

int get_last_modification_time(const char *path, uint64_t *timestamp) {
    char resolved_path[256] = {0};
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;

    char mount_point[256] = {0};
    char relative_path[256] = {0};
    separate_mount_point_and_path(path, mount_point, relative_path);

    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, mount_point) == 0) {
            // Found the mount point
            filesystem_t *fs = &filesystems[find_filesystem(mountpoints[i].type)];
            fs->set_read_only(mountpoints[i].flags & FLAG_READ_ONLY); // Set read-only mode if applicable
            fs->select(mountpoints[i].drive, mountpoints[i].partition); // Select the filesystem
            if (fs && fs->get_last_modification_time) {
                return fs->get_last_modification_time(relative_path, timestamp);
            }
            return -2; // Filesystem does not support last modification time retrieval
        }
    }
    return -1; // Mount point not found
}

void register_intree_filesystems() {
    fat32_register();
}
