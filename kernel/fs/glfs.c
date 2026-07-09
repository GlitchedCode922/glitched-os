#include "glfs.h"
#include "../vfs.h"
#include "../drivers/block.h"
#include "../drivers/chrdev.h"
#include "../error.h"
#include "../memory/mman.h"
#include <stddef.h>
#include <stdint.h>

static glfs_mount_t* mount;

int glfs_read_block(uint64_t block_number, void* buffer) {
    if (block_number >= mount->superblock.block_count) return -EIO;
    uint64_t absolute_block = 17 + mount->superblock.bitmap_size + block_number - 1;
    return read_sectors(mount->backing, absolute_block * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
}

int glfs_write_block(uint64_t block_number, void* buffer) {
    if (mount->read_only) return -EROFS;
    if (block_number >= mount->superblock.block_count) return -EIO;
    uint64_t absolute_block = 17 + mount->superblock.bitmap_size + block_number - 1;
    return write_sectors(mount->backing, absolute_block * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
}

int glfs_check(block_device_t device) {
    uint8_t buffer[GLFS_BLOCK_SIZE];
    int res = read_sectors(device, 16 * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
    if (res < 0) return res;
    glfs_superblock_t* superblock = (glfs_superblock_t*)buffer;
    if (memcmp(superblock->signature, "GlitchFS", 8) != 0) {
        return 0; // Not a GlitchedFS filesystem
    }
    return 1; // Valid GlitchedFS filesystem
}

void* glfs_mount(block_device_t device, int flags) {
    if (!glfs_check(device)) return NULL;
    mount = kmalloc(sizeof(glfs_mount_t));
    mount->backing = device;
    mount->read_only = flags & FLAG_READ_ONLY;

    // Read the superblock
    uint8_t buffer[GLFS_BLOCK_SIZE];
    int res = read_sectors(device, 16 * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
    if (res < 0) {
        kfree(mount);
        return NULL;
    }
    memcpy(&mount->superblock, buffer, sizeof(glfs_superblock_t));

    // Read the block bitmap
    mount->block_bitmap = kmalloc(mount->superblock.bitmap_size * GLFS_BLOCK_SIZE);
    for (uint64_t i = 0; i < mount->superblock.bitmap_size; i++) {
        res = read_sectors(device, (17 + i) * (GLFS_BLOCK_SIZE / 512), &mount->block_bitmap[i * GLFS_BLOCK_SIZE], GLFS_BLOCK_SIZE / 512);
        if (res < 0) {
            kfree(mount->block_bitmap);
            kfree(mount);
            return NULL;
        }
    }

    return mount;
}

void glfs_select(void* p_mount) {
    mount = p_mount;
}

int glfs_block_alloc(uint64_t* block_number) {
    if (mount->read_only) return -EROFS;
    for (uint64_t i = mount->superblock.next_free == 0 ? 0 : mount->superblock.next_free - 1; i < mount->superblock.block_count; i++) {
        uint8_t byte = mount->block_bitmap[i / 8];
        if (!(byte & (1 << (i % 8)))) {
            // Found a free block
            mount->block_bitmap[i / 8] |= (1 << (i % 8)); // Mark as used
            // Write the updated bitmap back to disk
            int res = write_sectors(mount->backing, (17  + (i / (GLFS_BLOCK_SIZE * 8))) * (GLFS_BLOCK_SIZE / 512), &mount->block_bitmap[(i / (GLFS_BLOCK_SIZE * 8)) * GLFS_BLOCK_SIZE], GLFS_BLOCK_SIZE / 512);
            if (res < 0) {
                mount->block_bitmap[i / 8] &= ~(1 << (i % 8));
                return res;
            }
            *block_number = i + 1; // Block numbers start at 1
            mount->superblock.next_free = *block_number + 1;
            uint8_t buffer[GLFS_BLOCK_SIZE];
            memcpy(buffer, &mount->superblock, sizeof(glfs_superblock_t));
            write_sectors(mount->backing, 16 * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
            return 0; // Success
        }
    }

    for (uint64_t i = 0; i < mount->superblock.next_free; i++) {
        uint8_t byte = mount->block_bitmap[i / 8];
        if (!(byte & (1 << (i % 8)))) {
            // Found a free block
            mount->block_bitmap[i / 8] |= (1 << (i % 8)); // Mark as used
            // Write the updated bitmap back to disk
            int res = write_sectors(mount->backing, (17  + (i / (GLFS_BLOCK_SIZE * 8))) * (GLFS_BLOCK_SIZE / 512), &mount->block_bitmap[(i / (GLFS_BLOCK_SIZE * 8)) * GLFS_BLOCK_SIZE], GLFS_BLOCK_SIZE / 512);
            if (res < 0) {
                mount->block_bitmap[i / 8] &= ~(1 << (i % 8));
                return res;
            }
            *block_number = i + 1; // Block numbers start at 1
            mount->superblock.next_free = *block_number + 1;
            uint8_t buffer[GLFS_BLOCK_SIZE];
            memcpy(buffer, &mount->superblock, sizeof(glfs_superblock_t));
            write_sectors(mount->backing, 16 * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
            return 0; // Success
        }
    }
    return -ENOSPC; // No space left on device
}

int glfs_block_free(uint64_t block_number) {
    if (mount->read_only) return -EROFS;
    if (block_number == 0 || block_number > mount->superblock.block_count) {
        return -EINVAL; // Invalid block number
    }
    uint64_t index = block_number - 1;
    mount->block_bitmap[index / 8] &= ~(1 << (index % 8)); // Mark as free
    // Write the updated bitmap back to disk
    int res = write_sectors(mount->backing, (17  + (index / (GLFS_BLOCK_SIZE * 8))) * (GLFS_BLOCK_SIZE / 512), &mount->block_bitmap[(index / (GLFS_BLOCK_SIZE * 8)) * GLFS_BLOCK_SIZE], GLFS_BLOCK_SIZE / 512);
    if (res < 0) {
        mount->block_bitmap[index / 8] |= (1 << (index % 8));
        return res;
    }
    if (block_number < mount->superblock.next_free) {
        mount->superblock.next_free = block_number;
        uint8_t buffer[GLFS_BLOCK_SIZE];
        memcpy(buffer, &mount->superblock, sizeof(glfs_superblock_t));
        write_sectors(mount->backing, 16 * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
    }
    return 0; // Success
}

int glfs_read_inode_block_ptrs(uint64_t inode_number, uint64_t offset, uint64_t* block_ptrs, size_t size) {
    glfs_inode_t inode;
    glfs_inode_continuation_t cont;
    int res = glfs_read_block(inode_number, &inode);
    if (res < 0) return res;

    size_t count = 0;
    size_t skip = (size_t)offset; // Number of pointers to skip

    const size_t inode_ptrs = (GLFS_BLOCK_SIZE - 128) / 8;
    const size_t cont_ptrs = GLFS_BLOCK_SIZE / 8 - 6;

    if (skip < inode_ptrs) {
        for (size_t i = skip; i < inode_ptrs && count < size; i++) {
            block_ptrs[count++] = inode.blocks[i];
        }
        skip = 0;
    } else {
        skip -= inode_ptrs;
    }

    if (count >= size) {
        return count;
    }

    uint64_t next_block = inode.next_inode_block;
    while (next_block != 0 && count < size) {
        res = glfs_read_block(next_block, &cont);
        if (res < 0) return res;

        if (skip < cont_ptrs) {
            for (size_t i = skip; i < cont_ptrs && count < size; i++) {
                block_ptrs[count++] = cont.blocks[i];
            }
            skip = 0;
        } else {
            skip -= cont_ptrs;
        }

        next_block = cont.next_inode_block;
    }

    return count;
}

int glfs_write_inode_block_ptrs(uint64_t inode_number, uint64_t offset, uint64_t* block_ptrs, size_t size) {
    if (mount->read_only) return -EROFS;
    glfs_inode_t inode;
    glfs_inode_continuation_t cont;
    int res = glfs_read_block(inode_number, &inode);
    if (res < 0) return res;

    size_t count = 0;
    size_t skip = (size_t)offset; // Number of pointers to skip

    const size_t inode_ptrs = (GLFS_BLOCK_SIZE - 128) / 8;
    const size_t cont_ptrs = GLFS_BLOCK_SIZE / 8 - 6;

    if (offset + size > inode.block_count) {
        inode.block_count = offset + size;
        int res = glfs_write_block(inode_number, &inode);
        if (res < 0) return res;
    }

    if (skip < inode_ptrs) {
        for (size_t i = skip; i < inode_ptrs && count < size; i++) {
            inode.blocks[i] = block_ptrs[count++];
        }
        res = glfs_write_block(inode_number, &inode);
        if (res < 0) return res;
        skip = 0;
    } else {
        skip -= inode_ptrs;
    }

    if (count >= size) {
        return count;
    }

    uint64_t previous_block = 0;
    uint64_t next_block = inode.next_inode_block;
    while (count < size) {
        if (next_block) {
            res = glfs_read_block(next_block, &cont);
            if (res < 0) return res;
        } else {
            uint64_t new_block;
            res = glfs_block_alloc(&new_block);
            if (res < 0) return res;
            if (previous_block) {
                cont.next_inode_block = new_block;
                res = glfs_write_block(previous_block, &cont);
                if (res < 0) return res;
            } else {
                inode.next_inode_block = new_block;
                res = glfs_write_block(inode_number, &inode);
                if (res < 0) return res;
            }
            next_block = new_block;
            memset(&cont, 0, GLFS_BLOCK_SIZE);
            res = glfs_write_block(next_block, &cont);
            if (res < 0) return res;
        }

        if (skip < cont_ptrs) {
            for (size_t i = skip; i < cont_ptrs && count < size; i++) {
                cont.blocks[i] = block_ptrs[count++];
            }
            res = glfs_write_block(next_block, &cont);
            if (res < 0) return res;
            skip = 0;
        } else {
            skip -= cont_ptrs;
        }

        previous_block = next_block;
        next_block = cont.next_inode_block;
    }

    return count;
}

int glfs_read_inode(uint64_t inode_number, uint8_t* buffer, uint64_t offset, uint64_t size) {
    if (!buffer) return -EINVAL;
    if (size == 0) return 0;
    glfs_inode_t inode;
    int res = glfs_read_block(inode_number, &inode);
    if (res < 0) return res;
    if (inode.type == GLFS_BLK) {
        block_device_t dev = {
            .major_number = major(inode.rdev),
            .minor_number = minor(inode.rdev),
        };
        return block_read(dev, offset, buffer, size);
    } else if (inode.type == GLFS_CHR) {
        char_device_t dev = {
            .major_number = major(inode.rdev),
            .minor_number = minor(inode.rdev),
        };
        return char_read(dev, offset, buffer, size);
    }
    if (offset + size > inode.size) {
        if (offset >= inode.size) return 0;
        size = inode.size - offset;
    }
    uint64_t first_block = offset / GLFS_BLOCK_SIZE;
    uint64_t in_block_offset = offset % GLFS_BLOCK_SIZE;
    uint64_t block_count = (size + in_block_offset + GLFS_BLOCK_SIZE - 1) / GLFS_BLOCK_SIZE;
    uint64_t bytes_read = 0;
    uint64_t* block_pointers = kmalloc(block_count * 8);
    res = glfs_read_inode_block_ptrs(inode_number, first_block, block_pointers, block_count);
    if (res < 0) {
        kfree(block_pointers);
        return res;
    }
    for (int i = 0; i < block_count; i++) {
        uint8_t block_buffer[GLFS_BLOCK_SIZE];
        res = glfs_read_block(block_pointers[i], block_buffer);
        if (res < 0) {
            kfree(block_pointers);
            return res;
        }
        for (int j = 0; j < GLFS_BLOCK_SIZE - in_block_offset && j < size - bytes_read; j++) {
            buffer[bytes_read + j] = block_buffer[in_block_offset + j];
        }

        uint64_t copied = GLFS_BLOCK_SIZE - in_block_offset;
        if (copied > size - bytes_read) copied = size - bytes_read;
        bytes_read += copied;

        in_block_offset = 0;
    }
    kfree(block_pointers);
    return bytes_read;
}

int glfs_write_inode(uint64_t inode_number, const uint8_t* buffer, uint64_t offset, uint64_t size) {
    if (mount->read_only) return -EROFS;
    if (!buffer) return -EINVAL;
    if (size == 0) return 0;
    glfs_inode_t inode;
    int res = glfs_read_block(inode_number, &inode);
    if (res < 0) return res;
    if (inode.type == GLFS_BLK) {
        block_device_t dev = {
            .major_number = major(inode.rdev),
            .minor_number = minor(inode.rdev),
        };
        return block_write(dev, offset, buffer, size);
    } else if (inode.type == GLFS_CHR) {
        char_device_t dev = {
            .major_number = major(inode.rdev),
            .minor_number = minor(inode.rdev),
        };
        return char_write(dev, offset, buffer, size);
    }
    // Allocate more blocks if required
    uint64_t blocks_required = (offset + size + GLFS_BLOCK_SIZE - 1) / GLFS_BLOCK_SIZE;
    if (blocks_required > inode.block_count) {
        uint64_t blocks_to_add = blocks_required - inode.block_count;
        uint64_t* pointers = kmalloc(blocks_to_add * 8);
        for (int i = 0; i < blocks_to_add; i++) {
            res = glfs_block_alloc(pointers + i);
            if (res < 0) {
                for (int j = 0; j < i; j++) {
                    glfs_block_free(pointers[j]);
                }
                kfree(pointers);
                return res;
            }
        }
        res = glfs_write_inode_block_ptrs(inode_number, inode.block_count, pointers, blocks_to_add);
        if (res < 0) {
            for (int i = 0; i < blocks_to_add; i++) {
                glfs_block_free(pointers[i]);
            }
            kfree(pointers);
            return res;
        }

        // Cannot free blocks on failure anymore, there is no way to remove pointers from inode
        res = glfs_read_block(inode_number, &inode);
        if (res < 0) {
            kfree(pointers);
            return res;
        }

        kfree(pointers);
        inode.block_count = blocks_required;
    }
    if (offset + size > inode.size) {
        inode.size = offset + size;
    }
    res = glfs_write_block(inode_number, &inode);
    if (res < 0) return res;

    uint64_t first_block = offset / GLFS_BLOCK_SIZE;
    uint64_t in_block_offset = offset % GLFS_BLOCK_SIZE;
    uint64_t block_count = (size + in_block_offset + GLFS_BLOCK_SIZE - 1) / GLFS_BLOCK_SIZE;
    uint64_t bytes_written = 0;
    uint64_t* block_pointers = kmalloc(block_count * 8);
    res = glfs_read_inode_block_ptrs(inode_number, first_block, block_pointers, block_count);
    if (res < 0) {
        kfree(block_pointers);
        return res;
    }
    for (int i = 0; i < block_count; i++) {
        uint8_t block_buffer[GLFS_BLOCK_SIZE];
        res = glfs_read_block(block_pointers[i], block_buffer);
        if (res < 0) {
            kfree(block_pointers);
            return res;
        }
        for (int j = 0; j < GLFS_BLOCK_SIZE - in_block_offset && j < size - bytes_written; j++) {
            block_buffer[in_block_offset + j] = buffer[bytes_written + j];
        }
        res = glfs_write_block(block_pointers[i], block_buffer);
        if (res < 0) {
            kfree(block_pointers);
            return res;
        }

        uint64_t copied = GLFS_BLOCK_SIZE - in_block_offset;
        if (copied > size - bytes_written) copied = size - bytes_written;
        bytes_written += copied;

        in_block_offset = 0;
    }
    kfree(block_pointers);
    return bytes_written;
}

int glfs_get_dirent(const char* path, glfs_dirent_ref_t* p_dirent) {
    if (!path) return -EINVAL;
    while (*path == '/') path++;
    glfs_inode_t current;
    uint64_t inode_block = 0;
    uint64_t next_inode_block = mount->superblock.root_inode;
    int dirent_index = 0;
    int res;
    while (*path) {
        char subdir[GLFS_MAX_FILENAME_LENGTH] = {0};
        int count = 0;
        while (*path != '/' && *path && count < GLFS_MAX_FILENAME_LENGTH) {
            subdir[count++] = *path++;
        }
        if (count == 0) {
            if (*path == '/') path++;  // Consume slash
            continue;
        }
        if (count == GLFS_MAX_FILENAME_LENGTH && *path && *path != '/') return -ENAMETOOLONG;
        inode_block = next_inode_block;
        res = glfs_read_block(inode_block, &current);
        if (res < 0) return res;
        if (current.type != GLFS_DIR) return -ENOENT;
        if (current.size == 0) return -ENOENT;
        if (current.size % 256 != 0) return -EIO;
        // Read directory
        glfs_dirent_t* dirents = kmalloc(current.size);
        res = glfs_read_inode(inode_block, (uint8_t*)dirents, 0, current.size);
        if (res < 0) {
            kfree(dirents);
            return res;
        }
        if (res != current.size) return -EIO;
        int found = 0;
        for (dirent_index = 0; dirent_index < current.size / 256; dirent_index++) {
            if (memcmp(dirents[dirent_index].name, subdir, GLFS_MAX_FILENAME_LENGTH) == 0) {
                // Found
                found = 1;
                next_inode_block = dirents[dirent_index].inodeptr;
                break;
            }
        }
        kfree(dirents);
        if (!found) return -ENOENT;
    }
    p_dirent->inode = inode_block;
    p_dirent->index = dirent_index;
    return 0;
}

int glfs_readdir(const char *path, int index, dirent_t *out) {
    if (!path) return -EINVAL;
    if (!out) return -EINVAL;
    if (index < 0) return -EINVAL;
    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(path, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dir = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dir, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        dir.inodeptr = mount->superblock.root_inode;
    }
    glfs_inode_t inode;
    res = glfs_read_block(dir.inodeptr, &inode);
    if (res < 0) return res;
    if (inode.type != GLFS_DIR) return -ENOTDIR;
    if (inode.size % 256 != 0) return -EIO;
    uint64_t dirent_count = inode.size / 256;
    if (index >= dirent_count) return 1;
    glfs_dirent_t dirent;
    res = glfs_read_inode(dir.inodeptr, (uint8_t*)&dirent, index * 256, 256);
    if (res < 0) return res;
    res = glfs_read_block(dirent.inodeptr, &inode);
    if (res < 0) return res;
    memcpy(out->name, dirent.name, GLFS_MAX_FILENAME_LENGTH);
    out->name[GLFS_MAX_FILENAME_LENGTH] = '\0'; // sizeof(dirent_t.name) = 256, GLFS max filename = 248
    switch (inode.type) {
        case GLFS_REG: out->type = DT_FILE; break;
        case GLFS_DIR: out->type = DT_DIR; break;
        case GLFS_BLK: out->type = DT_BLOCK; break;
        case GLFS_CHR: out->type = DT_CHAR; break;
        default: out->type = DT_UNKNOWN; break;
    }
    return 0;
}

int glfs_stat(const char *path, stat_t *out) {
    if (!path) return -EINVAL;
    if (!out) return -EINVAL;
    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(path, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dirent = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dirent, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        dirent.inodeptr = mount->superblock.root_inode;
    }
    glfs_inode_t inode;
    res = glfs_read_block(dirent.inodeptr, &inode);
    if (res < 0) return res;
    switch (inode.type) {
        case GLFS_REG: out->type = DT_FILE; break;
        case GLFS_DIR: out->type = DT_DIR; break;
        case GLFS_BLK: out->type = DT_BLOCK; break;
        case GLFS_CHR: out->type = DT_CHAR; break;
        default: out->type = DT_UNKNOWN; break;
    }
    out->size = inode.size;
    out->btime = inode.ctime;
    out->ctime = inode.ctime;
    out->mtime = inode.mtime;
    out->rdev = inode.rdev;
    return 0;
}

int glfs_read(const char *path, uint8_t *buffer, uint64_t offset, uint64_t size) {
    if (!path) return -EINVAL;
    if (!buffer) return -EINVAL;
    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(path, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dirent = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dirent, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        dirent.inodeptr = mount->superblock.root_inode;
    }
    return glfs_read_inode(dirent.inodeptr, buffer, offset, size);
}

int glfs_write(const char *path, const uint8_t *buffer, uint64_t offset, uint64_t size) {
    if (mount->read_only) return -EROFS;
    if (!buffer) return -EINVAL;
    if (!path) return -EINVAL;
    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(path, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dirent = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dirent, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        dirent.inodeptr = mount->superblock.root_inode;
    }
    return glfs_write_inode(dirent.inodeptr, buffer, offset, size);
}

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

static int _glfs_link(const char *path, uint64_t inode_number, uint64_t allow_dirs) {
    if (mount->read_only) return -EROFS;
    if (!path) return -EINVAL;
    stat_t st;
    if (stat(path, &st) >= 0) return -EEXIST;
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    size_t slash = len;
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }
    char dirname[1024];
    char filename[GLFS_MAX_FILENAME_LENGTH] = {0};
    if (slash == 0) {
        dirname[0] = '\0';
    } else {
        size_t dlen = slash - 1;
        if (dlen >= 1024) return -ENAMETOOLONG;
        memcpy(dirname, path, dlen);
        dirname[dlen] = '\0';
    }
    size_t flen = len - slash;
    if (flen > GLFS_MAX_FILENAME_LENGTH) return -ENAMETOOLONG;
    if (flen == 0) return -EINVAL;
    memcpy(filename, path + slash, flen);

    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(dirname, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dir = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dir, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        dir.inodeptr = mount->superblock.root_inode;
    }
    glfs_inode_t dir_inode;
    res = glfs_read_block(dir.inodeptr, &dir_inode);
    if (res < 0) return res;
    if (dir_inode.size % 256 != 0) return -EIO;

    // Read inode
    glfs_inode_t inode = {0};
    res = glfs_read_block(inode_number, &inode);
    if (res < 0) return res;

    if (inode.type == GLFS_DIR && !allow_dirs) return -EISDIR;

    // Write the dirent
    glfs_dirent_t new_dirent = {0};
    new_dirent.inodeptr = inode_number;
    memcpy(new_dirent.name, filename, GLFS_MAX_FILENAME_LENGTH);
    res = glfs_write_inode(dir.inodeptr, (uint8_t*)(&new_dirent), dir_inode.size, 256);
    if (res < 0) return res;

    // Increment inode refcount
    inode.refcount++;
    res = glfs_write_block(inode_number, &inode);
    if (res < 0) return res;

    return 0;
}

int glfs_mknod(const char *path, uint32_t type, dev_t dev) {
    if (mount->read_only) return -EROFS;
    if (!path) return -EINVAL;
    uint64_t inode_number;
    glfs_inode_t inode = {0};
    int res = glfs_block_alloc(&inode_number);
    if (res < 0) return res;
    uint64_t sig;
    memcpy(&sig, "GLFS_INO", 8);
    sig ^= inode_number;
    memcpy(&inode.signature, &sig, 8);
    inode.refcount = 0;
    inode.perms = 0755;
    switch (type) {
        case DT_FILE: inode.type = GLFS_REG; break;
        case DT_DIR: inode.type = GLFS_DIR; break;
        case DT_BLOCK: inode.type = GLFS_BLK; break;
        case DT_CHAR: inode.type = GLFS_CHR; break;
        default: glfs_block_free(inode_number); return -EINVAL;
    }
    inode.rdev = dev;
    res = glfs_write_block(inode_number, &inode);
    if (res < 0) {
        glfs_block_free(inode_number);
        return res;
    }
    res = _glfs_link(path, inode_number, 1);
    if (res < 0) glfs_block_free(inode_number);
    return res;
}

int glfs_create_file(const char *path) {
    return glfs_mknod(path, DT_FILE, 0);
}

int glfs_create_directory(const char *path) {
    return glfs_mknod(path, DT_DIR, 0);
}

int glfs_delete(const char *path) {
    if (mount->read_only) return -EROFS;
    if (!path) return -EINVAL;
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') {
        len--;
    }
    size_t slash = len;
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }
    char dirname[1024];
    if (slash == 0) {
        dirname[0] = '\0';
    } else {
        size_t dlen = slash - 1;
        if (dlen >= 1024) return -ENAMETOOLONG;
        memcpy(dirname, path, dlen);
        dirname[dlen] = '\0';
    }

    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(dirname, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dir = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dir, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        dir.inodeptr = mount->superblock.root_inode;
    }
    glfs_inode_t dir_inode;
    res = glfs_read_block(dir.inodeptr, &dir_inode);
    if (res < 0) return res;
    if (dir_inode.size % 256 != 0) return -EIO;

    res = glfs_get_dirent(path, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t dirent = {0};
    if (!dirent_ref.inode) return -EINVAL; // Cannot delete root
    res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&dirent, dirent_ref.index * 256, 256);
    if (res < 0) return res;
    glfs_inode_t inode;
    res = glfs_read_block(dirent.inodeptr, &inode);
    if (res < 0) return res;

    // Delete dirent
    glfs_dirent_t last_dirent;
    res = glfs_read_inode(dir.inodeptr, (uint8_t*)&last_dirent, dir_inode.size - 256, 256);
    if (res < 0) return res;
    if (memcmp(&last_dirent, &dirent, 256)) { // If last dirent is not the one to be deleted
        res = glfs_write_inode(dir.inodeptr, (uint8_t*)&last_dirent, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    }
    dir_inode.size -= 256;
    res = glfs_write_block(dir.inodeptr, &dir_inode);
    if (res < 0) return res;

    // Delete inode if refcount <= 1

    // In this section, there is no error handling
    // because the dirent is already removed,
    // and the inode can't be recovered on failure since its blocks
    // may have already been freed. The worst case is a stale inode or leaked blocks.
    if (inode.refcount <= 1) {
        for (int i = 0; i < sizeof(inode.blocks) / 8 && i < inode.block_count; i++) {
            if (inode.blocks[i] == 0) continue;
            glfs_block_free(inode.blocks[i]);
        }
        glfs_inode_continuation_t cont;
        cont.next_inode_block = inode.next_inode_block;
        while (cont.next_inode_block) {
            uint64_t current = cont.next_inode_block;
            res = glfs_read_block(current, &cont);
            if (res < 0) return res;
            for (int i = 0; i < sizeof(cont.blocks) / 8; i++) {
                if (cont.blocks[i] == 0) continue;
                glfs_block_free(cont.blocks[i]);
            }
            glfs_block_free(current);
        }
        inode = (glfs_inode_t){0};
        glfs_write_block(dirent.inodeptr, &inode);
        glfs_block_free(dirent.inodeptr);
    } else {
        inode.refcount--;
        glfs_write_block(dirent.inodeptr, &inode);
    }

    return 0;
}

int glfs_rename(const char *old_path, const char *new_path) {
    if (!old_path) return -EINVAL;
    if (!new_path) return -EINVAL;
    if (mount->read_only) return -EROFS;
    glfs_dirent_ref_t dirent_ref;
    int res = glfs_get_dirent(old_path, &dirent_ref);
    if (res < 0) return res;
    glfs_dirent_t old_dirent = {0};
    if (dirent_ref.inode) {
        res = glfs_read_inode(dirent_ref.inode, (uint8_t*)&old_dirent, dirent_ref.index * 256, 256);
        if (res < 0) return res;
    } else {
        old_dirent.inodeptr = mount->superblock.root_inode;
    }
    res = _glfs_link(new_path, old_dirent.inodeptr, 1);
    if (res < 0) return res;
    res = glfs_delete(old_path);
    if (res < 0) glfs_delete(new_path);
    return res;
}

void glfs_register() {
    filesystem_t glfs = {0};
    memcpy(glfs.name, "glfs", 5);
    glfs.case_sensitive = 1;
    glfs.requires_backing = 1;

    glfs.check = glfs_check;
    glfs.mount = glfs_mount;
    glfs.select = glfs_select;

    glfs.readdir = glfs_readdir;
    glfs.stat = glfs_stat;
    glfs.read = glfs_read;
    glfs.write = glfs_write;
    glfs.mknod = glfs_mknod;
    glfs.create_file = glfs_create_file;
    glfs.create_directory = glfs_create_directory;
    glfs.remove = glfs_delete;
    glfs.rename = glfs_rename;

    register_filesystem(glfs);
}
