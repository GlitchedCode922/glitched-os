#include "glfs.h"
#include <glfs/glfs.h>
#include "../vfs.h"
#include "../drivers/block.h"
#include "../drivers/chrdev.h"
#include "../drivers/timer.h"
#include "../error.h"
#include "../memory/mman.h"
#include "glfs/layout.h"
#include <stddef.h>
#include <stdint.h>

static glfs_mount_t* mount;

static int64_t glfs_backing_read_block(void* data, uint64_t block, void* buffer) {
    block_device_t dev = {
        .major_number = major((dev_t)data),
        .minor_number = minor((dev_t)data),
    };
    return read_sectors(dev, block * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
}

static int64_t glfs_backing_write_block(void* data, uint64_t block, const void* buffer) {
    block_device_t dev = {
        .major_number = major((dev_t)data),
        .minor_number = minor((dev_t)data),
    };
    return write_sectors(dev, block * (GLFS_BLOCK_SIZE / 512), buffer, GLFS_BLOCK_SIZE / 512);
}

glfs_backing_t create_backing(block_device_t dev) {
    void* d = (void*)makedev(dev.major_number, dev.minor_number);
    return (glfs_backing_t){
        .data = d,
        .read_block = glfs_backing_read_block,
        .write_block = glfs_backing_write_block,
        .time = get_time,
        .alloc = kmalloc,
        .free = kfree,
    };
}

int glfs_glue_check(block_device_t dev) {
    glfs_backing_t backing = create_backing(dev);
    return glfs_check(&backing);
}

void* glfs_glue_mount(block_device_t dev, int flags) {
    glfs_backing_t backing = create_backing(dev);
    mount = glfs_mount(&backing, flags & FLAG_READ_ONLY);
    return mount;
}

int glfs_glue_unmount(void* p_mount) {
    return glfs_unmount(p_mount);
}

void glfs_glue_select(void* p_mount) {
    mount = p_mount;
}

int glfs_glue_lookup(const char *path, uint64_t* inode) {
    return glfs_lookup(mount, path, inode);
}

int glfs_glue_open(uint64_t inode) {
    return glfs_open(mount, inode);
}

int glfs_glue_close(uint64_t inode) {
    return glfs_close(mount, inode);
}

int glfs_glue_readdir(uint64_t inode, int index, dirent_t *out) {
    glfs_readdir_entry_t output;
    int res = glfs_readdir(mount, inode, index, &output);
    if (res < 0) return res;
    memcpy(out->name, output.name, GLFS_MAX_FILENAME_LENGTH);
    out->name[GLFS_MAX_FILENAME_LENGTH] = '\0';
    switch (output.type) {
        case GLFS_REG: out->type = DT_FILE; break;
        case GLFS_DIR: out->type = DT_DIR; break;
        case GLFS_BLK: out->type = DT_BLOCK; break;
        case GLFS_CHR: out->type = DT_CHAR; break;
        default: out->type = DT_UNKNOWN; break;
    }
    return res;
}

int glfs_glue_stat(uint64_t inode, stat_t *out) {
    glfs_attr_t output;
    int res = glfs_getattr(mount, inode, &output);
    if (res < 0) return res;
    switch (output.type) {
        case GLFS_REG: out->type = DT_FILE; break;
        case GLFS_DIR: out->type = DT_DIR; break;
        case GLFS_BLK: out->type = DT_BLOCK; break;
        case GLFS_CHR: out->type = DT_CHAR; break;
        default: out->type = DT_UNKNOWN; break;
    }
    out->mtime = output.mtime;
    out->btime = output.ctime;
    out->ctime = output.ctime;
    out->rdev = output.rdev;
    out->size = output.size;
    return 0;
}

int glfs_glue_read(uint64_t inode, uint8_t *buffer, size_t offset, size_t size) {
    int res = glfs_read(mount, inode, buffer, offset, size);
    if (res == -ENODEV) {
        stat_t st;
        res = glfs_glue_stat(inode, &st);
        if (res < 0) return res;
        if (st.type == GLFS_BLK) {
            block_device_t dev = {
                .major_number = major(st.rdev),
                .minor_number = minor(st.rdev),
            };
            return block_read(dev, offset, buffer, size);
        } else if (st.type == GLFS_CHR) {
            char_device_t dev = {
                .major_number = major(st.rdev),
                .minor_number = minor(st.rdev),
            };
            return char_read(dev, offset, buffer, size);
        }
        return -ENODEV;
    }
    return res;
}

int glfs_glue_write(uint64_t inode, const uint8_t *buffer, size_t offset, size_t size) {
    int res = glfs_write(mount, inode, buffer, offset, size);
    if (res == -ENODEV) {
        stat_t st;
        res = glfs_glue_stat(inode, &st);
        if (res < 0) return res;
        if (st.type == GLFS_BLK) {
            block_device_t dev = {
                .major_number = major(st.rdev),
                .minor_number = minor(st.rdev),
            };
            return block_write(dev, offset, buffer, size);
        } else if (st.type == GLFS_CHR) {
            char_device_t dev = {
                .major_number = major(st.rdev),
                .minor_number = minor(st.rdev),
            };
            return char_write(dev, offset, buffer, size);
        }
        return -ENODEV;
    }
    return res;
}

int glfs_glue_mknod(const char *path, uint32_t type, dev_t dev) {
    switch (type) {
        case DT_FILE: type = GLFS_REG; break;
        case DT_DIR: type = GLFS_DIR; break;
        case DT_BLOCK: type = GLFS_BLK; break;
        case DT_CHAR: type = GLFS_CHR; break;
        default: return -EINVAL;
    }
    return glfs_mknod(mount, path, type, dev, 0777, 0, 0);
}

int glfs_glue_create_file(const char *path) {
    return glfs_glue_mknod(path, DT_FILE, 0);
}

int glfs_glue_create_directory(const char *path) {
    return glfs_glue_mknod(path, DT_DIR, 0);
}

int glfs_glue_delete(const char *path) {
    return glfs_delete(mount, path);
}

int glfs_glue_rename(const char *old_path, const char *new_path) {
    return glfs_rename(mount, old_path, new_path);
}

int glfs_glue_link(uint64_t inode, const char* link) {
    return glfs_link(mount, inode, link);
}

void glfs_register() {
    filesystem_t glfs = {0};
    memcpy(glfs.name, "glfs", 5);
    glfs.case_sensitive = 1;
    glfs.requires_backing = 1;

    glfs.check = glfs_glue_check;
    glfs.mount = glfs_glue_mount;
    glfs.unmount = glfs_glue_unmount;
    glfs.select = glfs_glue_select;

    glfs.lookup = glfs_glue_lookup;
    glfs.open = glfs_glue_open;
    glfs.close = glfs_glue_close;

    glfs.readdir = glfs_glue_readdir;
    glfs.stat = glfs_glue_stat;
    glfs.read = glfs_glue_read;
    glfs.write = glfs_glue_write;
    glfs.mknod = glfs_glue_mknod;
    glfs.create_file = glfs_glue_create_file;
    glfs.create_directory = glfs_glue_create_directory;
    glfs.remove = glfs_glue_delete;
    glfs.rename = glfs_glue_rename;
    glfs.link = glfs_glue_link;

    register_filesystem(glfs);
}
