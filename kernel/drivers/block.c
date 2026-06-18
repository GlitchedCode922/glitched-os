#include "block.h"
#include <stdint.h>
#include "../error.h"
#include "partitions.h"
#include "../vfs.h"
#include "../fs/devfs.h"

block_driver_t block_drivers[128] = {0};
int block_driver_count = 0;
int block_device_count = 0;

int read_sectors(block_device_t device, uint64_t lba, uint8_t *buffer, uint64_t count) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENOSYS;
    return block_drivers[device.major_number].read_sectors(device.minor_number, lba, buffer, count);
}

int write_sectors(block_device_t device, uint64_t lba, const uint8_t *buffer, uint64_t count) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENOSYS;
    return block_drivers[device.major_number].write_sectors(device.minor_number, lba, buffer, count);
}

int64_t get_device_size(block_device_t device) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENOSYS;
    return block_drivers[device.major_number].get_blockdev_size(device.minor_number);
}

int64_t get_sector_size(block_device_t device) {
    if (device.major_number == 0 || !block_drivers[device.major_number].present) return -ENOSYS;
    return block_drivers[device.major_number].get_sector_size(device.minor_number);
}

int block_read(block_device_t dev, uint64_t offset, uint8_t *buffer, size_t size) {
    uint64_t sector_size = get_sector_size(dev);

    uint64_t start_sector = offset / sector_size;
    uint64_t end_sector = (offset + size - 1) / sector_size;

    size_t out_off = 0;

    for (uint64_t s = start_sector; s <= end_sector; s++) {
        uint8_t sector[4096]; // OR dynamically sized fixed max sector

        int res = read_sectors(dev, s, sector, 1);
        if (res < 0) return res;

        uint64_t sector_start = s * sector_size;

        uint64_t from = (offset > sector_start) ? offset - sector_start : 0;
        uint64_t to = sector_size;
        if (out_off + (to - from) > size) {
            to = from + (size - out_off);
        }

        for (uint64_t i = from; i < to; i++) {
            buffer[out_off++] = sector[i];
        }
    }

    return size;
}

int block_write(block_device_t dev, uint64_t offset, const uint8_t *buffer, size_t size) {
    uint64_t sector_size = get_sector_size(dev);

    uint64_t start_sector = offset / sector_size;
    uint64_t end_sector = (offset + size - 1) / sector_size;

    size_t in_off = 0;

    for (uint64_t s = start_sector; s <= end_sector; s++) {
        uint8_t sector[4096];

        int res = read_sectors(dev, s, sector, 1);
        if (res < 0) return res;

        uint64_t sector_start = s * sector_size;

        uint64_t from = (offset > sector_start) ? offset - sector_start : 0;
        uint64_t to = sector_size;

        if (in_off + (to - from) > size) {
            to = from + (size - in_off);
        }

        for (uint64_t i = from; i < to; i++) {
            sector[i] = buffer[in_off++];
        }

        res = write_sectors(dev, s, sector, 1);
        if (res < 0) return res;
    }

    return size;
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
