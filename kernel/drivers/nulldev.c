#include "nulldev.h"
#include "chrdev.h"
#include "../memory/mman.h"
#include "../error.h"
#include <stdint.h>

int64_t null_write(int minor_number, uint64_t offset, const uint8_t *buffer, uint64_t size) {
    if (minor_number < 0 || minor_number > 1) return -ENODEV;
    return size;
}

static int64_t _null_read(uint64_t offset, uint8_t *buffer, uint64_t size) {
    return 0;
}

static int64_t _zero_read(uint64_t offset, uint8_t *buffer, uint64_t size) {
    memset(buffer, 0, size);
    return size;
}

int64_t null_read(int minor_number, uint64_t offset, uint8_t *buffer, uint64_t size) {
    if (minor_number == 0) {
        return _null_read(offset, buffer, size);
    } else if (minor_number == 1) {
        return _zero_read(offset, buffer, size);
    } else {
        return -ENODEV;
    }
}

int register_null_devices() {
    char_driver_t null_driver = {
        .read = null_read,
        .write = null_write,
    };
    int driver = register_char_driver(&null_driver);
    if (driver < 0) return driver;
    if (driver == 0) return -1;
    char_device_t null_device = {
        .major_number = driver,
        .minor_number = 0,
        .name = "null",
    };
    char_device_t zero_device = {
        .major_number = driver,
        .minor_number = 1,
        .name = "zero",
    };
    register_char_device(&null_device);
    register_char_device(&zero_device);
    return 0;
}
