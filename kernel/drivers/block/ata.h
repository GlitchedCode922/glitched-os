#pragma once

#include <stdint.h>

typedef struct {
    int exists;
    int type; // 0 for ATA, 1 for ATAPI
    uint16_t identify[256];
    uint64_t size;
    uint64_t sector_size;
} ata_device_t;

#define PRIMARY_BUS 0x1F0
#define SECONDARY_BUS 0x170

void scan_for_devices();
void select_drive(uint16_t bus_port, uint16_t disk);
ata_device_t detect_device(uint16_t bus_port, uint16_t disk);
int ata_read_sectors(int drive, uint64_t lba, uint8_t *buffer, uint64_t count);
int ata_write_sectors(int drive, uint64_t lba, const uint8_t *buffer, uint64_t count);
int ata_get_smart_data(int drive, uint8_t *buffer);
int64_t ata_get_drive_size(int drive);
int64_t ata_get_sector_size(int drive);
int ata_standby(int drive);
int ata_load_eject(int drive, uint8_t load);
int ata_supports_lba48(int drive);
void ata_register();
int is_atapi_device(int drive);
