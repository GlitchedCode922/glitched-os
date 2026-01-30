#include "partitions.h"
#include <stdint.h>

int is_partitioned(uint8_t disk) {
    return has_gpt(disk) || has_mbr(disk);
}

uint64_t get_partition_start(uint8_t disk, uint8_t partition) {
    if (has_gpt(disk)) {
        return gpt_get_partition_start(disk, partition);
    } else if (has_mbr(disk)) {
        return mbr_get_partition_start(disk, partition);
    }
    return -1;
}

uint64_t get_partition_size(uint8_t disk, uint8_t partition) {
    if (has_gpt(disk)) {
        return gpt_get_partition_size(disk, partition);
    } else if (has_mbr(disk)) {
        return mbr_get_partition_size(disk, partition);
    }
    return -1;
}

int read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (has_gpt(disk)) {
        return gpt_read_sectors_relative(disk, partition, lba, buffer, count);
    } else if (has_mbr(disk)) {
        return mbr_read_sectors_relative(disk, partition, lba, buffer, count);
    }
    return -1;
}

int write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (has_gpt(disk)) {
        return gpt_write_sectors_relative(disk, partition, lba, buffer, count);
    } else if (has_mbr(disk)) {
        return mbr_write_sectors_relative(disk, partition, lba, buffer, count);
    }
    return -1;
}
