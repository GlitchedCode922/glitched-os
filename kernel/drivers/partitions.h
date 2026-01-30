#pragma once
#include "partitions/mbr.h"
#include "partitions/gpt.h"

int is_partitioned(uint8_t disk);
uint64_t get_partition_start(uint8_t disk, uint8_t partition);
uint64_t get_partition_size(uint8_t disk, uint8_t partition);
int read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
int write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
