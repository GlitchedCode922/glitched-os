#pragma once
#include <stdint.h>

typedef struct {
    int disk;
    int partition;
    int64_t start;
    int64_t size;
} partition_t;

int is_partitioned(uint8_t disk);
int get_partition(uint8_t disk, uint8_t partition, partition_t* out);
int read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
int write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
