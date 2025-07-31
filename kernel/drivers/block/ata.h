#pragma once

#include <stdint.h>

typedef struct {
    int exists;
    int type; // 0 for ATA, 1 for ATAPI
    uint16_t identify[256];
} ata_device_t;

#define PRIMARY_BUS 0x1F0
#define SECONDARY_BUS 0x170

void scan_for_devices();
void select_drive(uint16_t bus_port, uint16_t disk);
ata_device_t detect_device(uint16_t bus_port, uint16_t disk);
int read_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count);
int write_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count);
int get_smart_data(uint8_t drive, uint8_t *buffer);
uint64_t get_drive_size(int drive);
void standby(int drive);
int supports_lba48(int drive);
