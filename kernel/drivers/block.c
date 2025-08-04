#include "block.h"
#include <stdint.h>

block_driver_t block_drivers[8] = {0};
block_device_t block_devices[32] = {0};
int block_driver_count = 0;
int block_device_count = 0;

int read_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (block_devices[drive].driver_index == 0) return -2;
    return block_drivers[block_devices[drive].driver_index].read(block_devices[drive].disk_index, lba, buffer, count);
}

int write_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (block_devices[drive].driver_index == 0) return -2;
    return block_drivers[block_devices[drive].driver_index].write(block_devices[drive].disk_index, lba, buffer, count);
}

uint64_t get_drive_size(uint8_t drive) {
    if (block_devices[drive].driver_index == 0) return 0;
    return block_drivers[block_devices[drive].driver_index].get_size(block_devices[drive].disk_index);
}

int get_smart_data(uint8_t drive, uint8_t *buffer) {
    if (block_devices[drive].driver_index == 0) return -2;
    return block_drivers[block_devices[drive].driver_index].get_smart_data(block_devices[drive].disk_index, buffer);
}

int standby(uint8_t drive) {
    if (block_devices[drive].driver_index == 0) return -2;
    block_drivers[block_devices[drive].driver_index].standby(block_devices[drive].disk_index);
    return 0;
}

int get_disk_index(uint8_t driver, uint8_t disk) {
    for (int i = 0; i < block_device_count; i++) {
        if (block_devices[i].driver_index == driver && block_devices[i].disk_index == disk) {
            return i;
        }
    }
    return -1; // Device not found
}

int register_block_driver(block_driver_t *driver) {
    if (block_driver_count >= 7) return -1; // Ensure we don't exceed the array bounds
    block_drivers[++block_driver_count] = *driver;
    return block_driver_count;
}

int register_block_device(block_device_t *device) {
    if (block_device_count >= 32) return -1; // Ensure we don't exceed the array bounds
    block_devices[block_device_count++] = *device;
    return block_device_count - 1;
}
