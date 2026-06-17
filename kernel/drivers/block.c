#include "block.h"
#include <stdint.h>
#include "../error.h"
#include "partitions.h"

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

int register_block_driver(block_driver_t *driver) {
    if (block_driver_count >= 127) return -ENOSPC; // Ensure we don't exceed the array bounds
    driver->present = 1;
    block_drivers[++block_driver_count] = *driver;
    return block_driver_count;
}

int register_block_device(block_device_t *device) {
    if (!device->is_partition) detect_partitions(*device);
    return 0; // Temporarily no-op
}
