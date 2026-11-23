#pragma once
#include <stdint.h>
#include "io/pci.h"

extern int nvme_block_driver_index;

int nvme_read_sectors(int drive_number, uint64_t lba, uint8_t *buffer, uint64_t count);
int nvme_write_sectors(int drive_number, uint64_t lba, const uint8_t *buffer, uint64_t count);
int64_t nvme_get_drive_size(int drive_number);
int64_t nvme_get_block_size(int drive_number);
void nvme_init(pci_device_t device);
void nvme_register();
