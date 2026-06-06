#include "vfs.h"
#include "memory/mman.h"
#include "usermode/scheduler.h"
#include "error.h"
#include "fs/fat.h"
#include <stddef.h>
#include <stdint.h>

filesystem_t filesystems[24] = {0};
mountpoint_t* root = NULL;

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
        return -ENOSPC; // Maximum number of filesystems reached
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

static int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

static int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = (s1[i] >= 'A' && s1[i] <= 'Z') ? (s1[i] + 32) : s1[i];
        char c2 = (s2[i] >= 'A' && s2[i] <= 'Z') ? (s2[i] + 32) : s2[i];
        if (c1 != c2) {
            return (unsigned char)c1 - (unsigned char)c2;
        }
        if (c1 == '\0') {
            return 0;
        }
    }
    return 0;
}

static void make_absolute(const char *path, char *out) {
    if (path[0] == '/') {
        strncpy(out, path, MAX_PATH - 1);
        out[MAX_PATH - 1] = '\0';
        return;
    }

    size_t i = 0;

    // copy current_task->wd
    for (size_t j = 0; current_task->wd[j] && i < MAX_PATH - 1; j++) {
        out[i++] = current_task->wd[j];
    }

    // add slash if needed
    if (i == 0 || out[i - 1] != '/') {
        out[i++] = '/';
    }

    // append path
    for (size_t j = 0; path[j] && i < MAX_PATH - 1; j++) {
        out[i++] = path[j];
    }

    out[i] = '\0';
}

// Resolve path to canonical form (removes redundant slashes, handles "." and "..")
static void resolve_path(const char *path, char *out) {
    size_t len = 0;

    // Always start with root
    out[len++] = '/';
    out[len] = '\0';

    if (!path || *path == '\0') {
        return;
    }

    // Skip leading slashes
    while (*path == '/') {
        path++;
    }

    while (*path != '\0' && len < MAX_PATH - 1) {

        // Handle "."
        if (path[0] == '.' && (path[1] == '/' || path[1] == '\0')) {
            path++;
            continue;
        }

        // Handle ".."
        if (path[0] == '.' && path[1] == '.' &&
            (path[2] == '/' || path[2] == '\0')) {

            // Remove last component (but keep root "/")
            if (len > 1) {
                // remove trailing slash if present
                if (out[len - 1] == '/') {
                    len--;
                }

                // backtrack to previous slash
                while (len > 1 && out[len - 1] != '/') {
                    len--;
                }

                out[len] = '\0';
            }

            path += 2;
            if (*path == '/') path++;
            continue;
        }

        // Ensure single slash separation
        if (out[len - 1] != '/') {
            out[len++] = '/';
        }

        // Copy component
        while (*path && *path != '/' && len < MAX_PATH - 1) {
            out[len++] = *path++;
        }

        out[len] = '\0';

        // Skip extra slashes
        while (*path == '/') {
            path++;
        }
    }

    // Ensure null termination
    out[len] = '\0';
}

static mountpoint_t* find_mountpoint(const char *path, char *remaining_path) {
    char absolute_path[MAX_PATH];
    char resolved_path[MAX_PATH];
    char* p = resolved_path;
    make_absolute(path, absolute_path);
    resolve_path(absolute_path, resolved_path);

    mountpoint_t* current = root;
    mountpoint_t* m = root->children;
    while (m) {
        size_t mount_len = strlen(m->mount_point);
        if (filesystems[m->type].case_sensitive) {
            if (strncmp(m->mount_point, p, mount_len) == 0 && (p[mount_len] == '/' || p[mount_len] == '\0')) {
                current = m;
                p += mount_len;
                while (*p == '/') p++; // Skip any slashes after the mount point
                m = current->children; // Check for nested mounts
                continue;
            }
        } else {
            if (strncasecmp(m->mount_point, p, mount_len) == 0 && (p[mount_len] == '/' || p[mount_len] == '\0')) {
                current = m;
                p += mount_len;
                while (*p == '/') p++; // Skip any slashes after the mount point
                m = current->children; // Check for nested mounts
                continue;
            }
        }
        m = m->next;
    }

    if (remaining_path) {
        strcpy(remaining_path, p);
    }

    return current; // Return the best match found
}

int mount_filesystem(const char *path, const char *type, int drive, int partition, int flags) {
    if (filesystem_count == 0) {
        return -ENOENT; // No filesystems registered
    }

    int fs_index = -1;
    for (int i = 0; i < filesystem_count; i++) {
        if (strcmp(filesystems[i].name, type) == 0) {
            fs_index = i;
            break;
        }
    }

    if (fs_index == -1) {
        return -EINVAL; // Filesystem type not found
    }

    if (!filesystems[fs_index].check(drive, partition)) {
        return -EINVAL; // Filesystem check failed
    }

    char remaining_path[MAX_PATH];
    mountpoint_t* parent = find_mountpoint(path, remaining_path);
    if (!parent) {
        return -ENOENT; // Parent mount point not found
    }

    if (remaining_path[0] == '\0') {
        return -EBUSY; // Already mounted
    }

    mountpoint_t* new_mount = (mountpoint_t*)kmalloc(sizeof(mountpoint_t));
    new_mount->drive = drive;
    new_mount->partition = partition;
    new_mount->flags = flags;
    new_mount->type = fs_index;
    strncpy(new_mount->mount_point, remaining_path, sizeof(new_mount->mount_point) - 1);
    new_mount->mount_point[sizeof(new_mount->mount_point) - 1] = '\0';
    new_mount->parent = parent;
    new_mount->children = NULL;

    // Add to parent's children list
    new_mount->next = parent->children;
    parent->children = new_mount;

    return 0; // Success
}

int mount_root_filesystem(const char *type, int drive, int partition, int flags) {
    if (filesystem_count == 0) {
        return -ENOENT; // No filesystems registered
    }

    int fs_index = -1;
    for (int i = 0; i < filesystem_count; i++) {
        if (strcmp(filesystems[i].name, type) == 0) {
            fs_index = i;
            break;
        }
    }

    if (fs_index == -1) {
        return -EINVAL; // Filesystem type not found
    }

    if (!filesystems[fs_index].check(drive, partition)) {
        return -EINVAL; // Filesystem check failed
    }

    root = (mountpoint_t*)kmalloc(sizeof(mountpoint_t));
    root->drive = drive;
    root->partition = partition;
    root->flags = flags;
    root->type = fs_index;
    strcpy(root->mount_point, "/");
    root->parent = NULL;
    root->children = NULL;

    return 0; // Success
}

int unmount_filesystem(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount || mount == root || remaining_path[0] != '\0') {
        return -EINVAL; // Mount point not found or trying to unmount root
    }
    mountpoint_t* parent = mount->parent;
    if (parent->children == mount) {
        parent->children = mount->next;
    } else {
        mountpoint_t* sibling = parent->children;
        while (sibling && sibling->next != mount) {
            sibling = sibling->next;
        }
        if (sibling) {
            sibling->next = mount->next;
        }
    }
    kfree(mount);
    return 0; // Success
}

int unmount_all_filesystems() {
    // Recursively free all mount points
    mountpoint_t* m = root->children;
    while (m) {
        mountpoint_t* next = m->next;
        if (m->children) {
            mountpoint_t* child = m->children;
            while (child) {
                mountpoint_t* next_child = child->next;
                kfree(child);
                child = next_child;
            }
        }
        kfree(m);
        m = next;
    }
    return 0; // Success
}

int list_directory(const char *path, char *element, uint64_t element_index) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->list) {
        return -ENOSYS; // List operation not supported by this filesystem
    }
    return fs->list(remaining_path, element, element_index);
}

int exists(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return 0; // Mount point not found, so it doesn't exist
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->exists) {
        return 0; // Exists operation not supported by this filesystem
    }
    return fs->exists(remaining_path);
}

int is_directory(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->is_directory) {
        return -ENOSYS; // Is_directory operation not supported by this filesystem
    }
    return fs->is_directory(remaining_path);
}

uint64_t get_file_size(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->get_file_size) {
        return -ENOSYS; // Get_file_size operation not supported by this filesystem
    }
    return fs->get_file_size(remaining_path);
}

int read_file(const char *path, uint8_t *buffer, size_t offset, size_t size) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->read) {
        return -EINVAL; // Read operation not supported by this filesystem
    }
    return fs->read(remaining_path, buffer, offset, size);
}

int write_file(const char *path, const uint8_t *buffer, size_t offset, size_t size) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->write) {
        return -ENOSYS; // Write operation not supported by this filesystem
    }
    return fs->write(remaining_path, buffer, offset, size);
}

int remove_file(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->remove) {
        return -ENOSYS; // Remove operation not supported by this filesystem
    }
    return fs->remove(remaining_path);
}

int rename_file(const char *old_path, const char *new_path) {
    char old_remaining_path[MAX_PATH];
    char new_remaining_path[MAX_PATH];
    mountpoint_t* old_mount = find_mountpoint(old_path, old_remaining_path);
    mountpoint_t* new_mount = find_mountpoint(new_path, new_remaining_path);
    if (old_mount != new_mount) {
        return -ENOENT; // Cannot rename across different filesystems
    }
    filesystem_t* old_fs = &filesystems[old_mount->type];
    old_fs->select(old_mount->drive, old_mount->partition);
    old_fs->set_read_only(old_mount->flags & FLAG_READ_ONLY);
    if (!old_fs->rename) {
        return -ENOSYS; // Rename operation not supported by this filesystem
    }
    return old_fs->rename(old_remaining_path, new_remaining_path);
}

int create_file(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->create_file) {
        return -ENOSYS; // Create_file operation not supported by this filesystem
    }
    return fs->create_file(remaining_path);
}

int create_directory(const char *path) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->create_directory) {
        return -ENOSYS; // Create_directory operation not supported by this filesystem
    }
    return fs->create_directory(remaining_path);
}

int get_creation_time(const char *path, uint64_t *timestamp) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->get_creation_time) {
        return -ENOSYS; // Get_creation_time operation not supported by this filesystem
    }
    return fs->get_creation_time(remaining_path, timestamp);
}

int get_last_modification_time(const char *path, uint64_t *timestamp) {
    char remaining_path[MAX_PATH];
    mountpoint_t* mount = find_mountpoint(path, remaining_path);
    if (!mount) {
        return -ENOENT; // Mount point not found
    }
    filesystem_t* fs = &filesystems[mount->type];
    fs->select(mount->drive, mount->partition);
    fs->set_read_only(mount->flags & FLAG_READ_ONLY);
    if (!fs->get_last_modification_time) {
        return -ENOSYS; // Get_last_modification_time operation not supported by this filesystem
    }
    return fs->get_last_modification_time(remaining_path, timestamp);
}

void register_intree_filesystems() {
    fat_register();
}

void getcwd(char* buffer, size_t len) {
    strncpy(buffer, current_task->wd, len - 1);
    buffer[len - 1] = '\0';
}

int chdir(char* path) {
    char absolute_path[MAX_PATH];
    char resolved_path[MAX_PATH];
    make_absolute(path, absolute_path);
    resolve_path(absolute_path, resolved_path);

    int is_dir_result = is_directory(resolved_path);
    if (is_dir_result < 0) {
        return is_dir_result; // Propagate error code from is_directory
    } else if (is_dir_result == 0) {
        return -ENOTDIR; // Not a directory
    }
    strncpy(current_task->wd, resolved_path, MAX_PATH - 1);
    return 0; // Success
}
