#include "block.h"

block_driver_t block_drivers[8] = {0};
block_device_t block_devices[32] = {0};

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

void register_block_driver(block_driver_t *driver) {
    if (driver->driver_index >= 8 || driver->driver_index == 0) return; // Ensure we don't exceed the array bounds
    block_drivers[driver->driver_index] = *driver;
}

void register_block_device(block_device_t *device) {
    if (device->disk_index >= 32 || device->driver_index >= 8) return; // Ensure we don't exceed the array bounds
    block_devices[device->disk_index] = *device;
}
