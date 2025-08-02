#pragma once

#include <stdint.h>
#include "ata.h"

ata_device_t detect_packet_device(uint16_t bus_port, uint16_t disk);
int atapi_read_sectors(uint8_t drive, uint64_t lba, uint8_t* buffer, uint32_t count);
