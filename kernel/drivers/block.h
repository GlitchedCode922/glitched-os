#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int present;
    int (*read_sectors)(int minor_number, uint64_t lba, uint8_t *buffer, uint64_t count);
    int (*write_sectors)(int minor_number, uint64_t lba, const uint8_t *buffer, uint64_t count);
    int64_t (*get_blockdev_size)(int minor_number);
    int64_t (*get_sector_size)(int minor_number);
} block_driver_t;

typedef struct {
    uint32_t major_number;
    uint32_t minor_number;
    char* name;
    int is_partition;
} block_device_t;

int read_sectors(block_device_t device, uint64_t lba, uint8_t *buffer, uint64_t count);
int write_sectors(block_device_t device, uint64_t lba, const uint8_t *buffer, uint64_t count);
int block_read(block_device_t dev, uint64_t offset, uint8_t *buffer, size_t size);
int block_write(block_device_t dev, uint64_t offset, const uint8_t *buffer, size_t size);
int64_t get_blockdev_size(block_device_t device);
int64_t get_sector_size(block_device_t device);
int register_block_driver(block_driver_t *driver);
int register_block_device(block_device_t *device);
