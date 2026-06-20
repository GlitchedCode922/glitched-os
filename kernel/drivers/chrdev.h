#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int present;
    int (*read)(int minor_number, uint64_t offset, uint8_t *buffer, uint64_t size);
    int (*write)(int minor_number, uint64_t offset, const uint8_t *buffer, uint64_t size);
} char_driver_t;

typedef struct {
    uint32_t major_number;
    uint32_t minor_number;
    char* name;
} char_device_t;

int char_read(char_device_t device, uint64_t offset, uint8_t *buffer, uint64_t size);
int char_write(char_device_t device, uint64_t offset, const uint8_t *buffer, uint64_t size);
int register_char_driver(char_driver_t *driver);
int register_char_device(char_device_t *device);
