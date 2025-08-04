#pragma once
#include <stdint.h>

typedef struct {
    int (*read)(uint8_t disk, uint64_t lba, uint8_t *buffer, uint16_t count);
    int (*write)(uint8_t disk, uint64_t lba, uint8_t *buffer, uint16_t count);
    uint64_t (*get_size)(uint8_t disk);
    int (*get_smart_data)(uint8_t disk, uint8_t *buffer);
    void (*standby)(uint8_t disk);
} block_driver_t;

typedef struct {
    uint8_t driver_index;
    uint8_t disk_index;
} block_device_t;

int read_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count);
int write_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count);
uint64_t get_drive_size(uint8_t drive);
int get_smart_data(uint8_t drive, uint8_t *buffer);
int standby(uint8_t drive);
int get_disk_index(uint8_t driver, uint8_t disk);
int register_block_driver(block_driver_t *driver);
int register_block_device(block_device_t *device);
