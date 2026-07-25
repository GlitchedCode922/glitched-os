#include "chrdev.h"
#include "../fs/devfs.h"
#include "../vfs.h"
#include "../error.h"
#include <stdint.h>

char_driver_t char_drivers[128] = {0};
int char_driver_count = 0;

int register_char_driver(char_driver_t *driver) {
    if (char_driver_count >= 127) return -ENOSPC; // Ensure we don't exceed the array bounds
    driver->present = 1;
    char_drivers[++char_driver_count] = *driver;
    return char_driver_count;
}

int register_char_device(char_device_t *device) {
    if (device->name) {
        devfs_mknod(
            device->name,
            DT_CHAR,
            makedev(device->major_number, device->minor_number)
        );
    }
    return 0;
}

int64_t char_read(char_device_t device, uint64_t offset, uint8_t *buffer, uint64_t size) {
    if (device.major_number == 0 || !char_drivers[device.major_number].present) return -ENODEV;
    if (!char_drivers[device.major_number].read) return -ENOSYS;
    return char_drivers[device.major_number].read(device.minor_number, offset, buffer, size);
}

int64_t char_write(char_device_t device, uint64_t offset, const uint8_t *buffer, uint64_t size) {
    if (device.major_number == 0 || !char_drivers[device.major_number].present) return -ENODEV;
    if (!char_drivers[device.major_number].write) return -ENOSYS;
    return char_drivers[device.major_number].write(device.minor_number, offset, buffer, size);
}

int char_ioctl(char_device_t device, uint64_t request, uint64_t arg) {
    if (device.major_number == 0 || !char_drivers[device.major_number].present) return -ENODEV;
    if (!char_drivers[device.major_number].ioctl) return -ENOTTY;
    return char_drivers[device.major_number].ioctl(device.minor_number, request, arg);
}
