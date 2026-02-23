#include "mount.h"
#include "memory/mman.h"
#include "usermode/scheduler.h"
#include "fs/fat.h"
#include <stddef.h>
#include <stdint.h>

filesystem_t filesystems[24] = {0};
mountpoint_t mountpoints[48] = {0};

int filesystem_count = 0;

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

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

static char *strncpy(char *dest, const char *src, size_t len) {
    char *ret = dest;
    int i = 0;
    while ((i++ < len) && (*dest++ = *src++)); // copy until '\0'
    return ret;
}

static void strcat(char *dest, const char *src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

// Resolve relative path to absolute
char* resolve_path(char *rel_path) {
    static char temp[MAX_PATH + 1];
    char *stack[MAX_PATH];
    int top = 0;

    // If path starts with '/', use it as absolute
    if (rel_path[0] == '/') {
        return rel_path;
    } else {
        strncpy(temp, current_task->wd, MAX_PATH);
        temp[MAX_PATH] = '\0';
        size_t len = strlen(temp);
        if (len > 0 && temp[len - 1] != '/') temp[len++] = '/';
        size_t i = 0;
        while (rel_path[i] && len < MAX_PATH - 1) {
            temp[len++] = rel_path[i++];
        }
        temp[len] = '\0';
    }

    // Manual split by '/'
    char *p = temp;
    while (*p) {
        // Skip consecutive '/'
        while (*p == '/') p++;

        // Extract next component
        static char component[128];
        int ci = 0;
        while (*p && *p != '/' && ci < 127) {
            component[ci++] = *p++;
        }
        component[ci] = '\0';

        if (ci == 0) break;

        if (component[0] == '.' && component[1] == '\0') {
            // ignore '.'
        } else if (component[0] == '.' && component[1] == '.' && component[2] == '\0') {
            if (top > 0) top--; // go up one directory
        } else {
            stack[top++] = component; // push
        }
    }
    return temp;
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
    char path_tree[256][256] = {{0}};
    int path_index = 0;
    const char *p = path;

    int is_absolute = (*p == '/');

    // Split into segments
    while (*p) {
        if (*p == '/') {
            p++;
            continue;
        }
        char *segment = path_tree[path_index];
        while (*p && *p != '/') {
            *segment++ = *p++;
        }
        *segment = '\0';
        path_index++;
    }

    // Process segments, resolving "." and ".."
    int resolved_count = 0;
    for (int i = 0; i < path_index; i++) {
        if (strcmp(path_tree[i], ".") == 0) {
            continue; // skip
        } else if (strcmp(path_tree[i], "..") == 0) {
            if (resolved_count > 0) {
                resolved_count--;  // pop last valid dir
            } else if (!is_absolute) {
                // relative path, keep ".."
                strcpy(path_tree[resolved_count++], "..");
            }
        } else {
            strcpy(path_tree[resolved_count++], path_tree[i]);
        }
    }

    // Rebuild resolved path
    int resolved_index = 0;
    if (is_absolute) {
        resolved_path[resolved_index++] = '/';
    }

    for (int i = 0; i < resolved_count; i++) {
        if (i > 0) {
            resolved_path[resolved_index++] = '/';
        }
        size_t len = strlen(path_tree[i]);
        memcpy(resolved_path + resolved_index, path_tree[i], len);
        resolved_index += len;
    }

    if (resolved_index == 0) {
        resolved_path[resolved_index++] = is_absolute ? '/' : '.';
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
    for (int i = 0; i < 48; i++) {
        if (strcmp(mountpoints[i].mount_point, path) == 0) {
            mountpoints[i].mount_point[0] = '\0'; // Clear the mount point
            return 0; // Success
        }
    }
    return -1; // Mount point not found
}

int unmount_all_filesystems() {
    for (int i = 1; i < 48; i++) { // 1 is set here to not unmount root
        mountpoints[i].mount_point[0] = '\0'; // Clear all mount points
    }
    return 0; // Success
}

int list_directory(const char *path, char *element, uint64_t element_index) {
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
                return fs->create_file(relative_path);
            }
            return -2; // Filesystem does not support file creation
        }
    }
    return -1; // Mount point not found
}

int create_directory(const char *path) {
    path = resolve_path((char*)path);
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
                return fs->create_directory(relative_path);
            }
            return -2; // Filesystem does not support directory creation
        }
    }
    return -1; // Mount point not found
}

int get_creation_time(const char *path, uint64_t *timestamp) {
    path = resolve_path((char*)path);
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
    path = resolve_path((char*)path);
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
    fat_register();
}

void getcwd(char *buffer, size_t len) {
    strncpy(buffer, current_task->wd, len);
}

int chdir(char *path) {
    if (strlen(path) >= MAX_PATH) return -2;
    path = resolve_path(path);
    char resolved_path[1024];
    resolve_dot_or_dotdot(path, resolved_path);
    path = resolved_path;
    if (!exists(path)) return -1;
    strcpy(current_task->wd, path);
    return 0;
}
