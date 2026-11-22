#include "drivers/block.h"
#include "error.h"
#include "drivers/partitions.h"
#include "vfs.h"
#include "fs/devfs.h"
#include <stdint.h>

block_driver_t block_drivers[128] = {0};
int block_driver_count = 0;

int read_sectors(block_device_t device, uint64_t lba, uint8_t *buffer, uint64_t count) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENODEV;
    return block_drivers[device.major_number].read_sectors(device.minor_number, lba, buffer, count);
}

int write_sectors(block_device_t device, uint64_t lba, const uint8_t *buffer, uint64_t count) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENODEV;
    return block_drivers[device.major_number].write_sectors(device.minor_number, lba, buffer, count);
}

int64_t get_device_size(block_device_t device) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENODEV;
    return block_drivers[device.major_number].get_blockdev_size(device.minor_number);
}

int64_t get_sector_size(block_device_t device) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENODEV;
    return block_drivers[device.major_number].get_sector_size(device.minor_number);
}

int block_ioctl(block_device_t device, uint64_t request, uint64_t arg) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENODEV;
    if (!block_drivers[device.major_number].ioctl) return -ENOTTY;
    return block_drivers[device.major_number].ioctl(device.minor_number, request, arg);
}

int64_t block_read(block_device_t dev, uint64_t offset, uint8_t* buffer, size_t size) {
    if (dev.major_number == 0 || !block_drivers[dev.major_number].present) return -ENODEV;
    if (size == 0) return 0;

    int64_t device_size = get_device_size(dev);
    if (device_size < 0) return device_size;
    if (offset >= (uint64_t)device_size) return 0;

    uint64_t remaining = (uint64_t)device_size - offset;
    if (size > remaining) size = (size_t)remaining;
    if (size == 0) return 0;

    int64_t sector_size_signed = get_sector_size(dev);
    if (sector_size_signed <= 0 || sector_size_signed > 4096) return -EINVAL;

    uint64_t sector_size = (uint64_t)sector_size_signed;
    uint64_t start_sector = offset / sector_size;
    uint64_t end_sector = (offset + size - 1) / sector_size;

    size_t out_off = 0;

    for (uint64_t s = start_sector; s <= end_sector; s++) {
        uint8_t sector[sector_size];

        int res = read_sectors(dev, s, sector, 1);
        if (res < 0) return out_off ? out_off : res;

        uint64_t sector_start = s * sector_size;
        uint64_t from = offset > sector_start ? offset - sector_start : 0;
        uint64_t to = sector_size;

        if (to - from > size - out_off) to = from + (size - out_off);

        size_t amount = (size_t)(to - from);
        for (size_t i = 0; i < amount; i++) buffer[out_off++] = sector[from + i];
    }

    return out_off;
}

int64_t block_write(block_device_t dev, uint64_t offset, const uint8_t* buffer, size_t size) {
    if (dev.major_number == 0 || !block_drivers[dev.major_number].present) return -ENODEV;
    if (size == 0) return 0;

    int64_t device_size = get_device_size(dev);
    if (device_size < 0) return device_size;
    if (offset >= (uint64_t)device_size) return 0;

    uint64_t remaining = (uint64_t)device_size - offset;
    if (size > remaining) size = (size_t)remaining;
    if (size == 0) return 0;

    int64_t sector_size_signed = get_sector_size(dev);
    if (sector_size_signed <= 0 || sector_size_signed > 4096) return -EINVAL;

    uint64_t sector_size = (uint64_t)sector_size_signed;
    uint64_t start_sector = offset / sector_size;
    uint64_t end_sector = (offset + size - 1) / sector_size;

    size_t in_off = 0;

    for (uint64_t s = start_sector; s <= end_sector; s++) {
        uint8_t sector[4096];

        uint64_t sector_start = s * sector_size;
        uint64_t from = offset > sector_start ? offset - sector_start : 0;
        uint64_t to = sector_size;

        if (to - from > size - in_off) to = from + (size - in_off);

        size_t amount = (size_t)(to - from);

        if (from != 0 || to != sector_size) {
            int res = read_sectors(dev, s, sector, 1);
            if (res < 0) return in_off ? in_off : res;
        }

        if (from == 0 && to == sector_size) {
            for (size_t i = 0; i < amount; i++) sector[i] = buffer[in_off++];
        } else {
            for (size_t i = 0; i < amount; i++) sector[from + i] = buffer[in_off++];
        }

        int res = write_sectors(dev, s, sector, 1);
        if (res < 0) return in_off >= amount ? in_off - amount : res;
    }

    return in_off;
}

int register_block_driver(block_driver_t *driver) {
    if (block_driver_count >= 127) return -ENOSPC; // Ensure we don't exceed the array bounds
    driver->present = 1;
    block_drivers[++block_driver_count] = *driver;
    return block_driver_count;
}

int register_block_device(block_device_t *device) {
    if (!device->is_partition) detect_partitions(*device);
    if (device->name) {
        devfs_mknod(
            device->name,
            DT_BLOCK,
            makedev(device->major_number, device->minor_number)
        );
    }
    return 0;
}
