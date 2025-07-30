#pragma once

#include <stdint.h>

typedef struct {
    int exists;
    int index;
    int type; // 0 for ATA, 1 for ATAPI
    uint32_t identify[256];
} ata_device_t;

#define PRIMARY_BUS 0x1F0
#define SECONDARY_BUS 0x170

void scan_for_devices();
void select_drive(uint16_t bus_port, uint16_t disk);
ata_device_t detect_device(uint16_t bus_port, uint16_t disk);
int read_sectors(uint8_t drive, uint32_t lba, uint8_t *buffer, uint8_t count);
int write_sectors(uint8_t drive, uint32_t lba, uint8_t *buffer, uint8_t count);
int get_smart_data(uint8_t drive, uint8_t *buffer);
void standby(int drive);
