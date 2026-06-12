#include "partitions.h"
#include "partitions/gpt.h"
#include "partitions/mbr.h"
#include "block.h"
#include "../error.h"
#include <stdint.h>

static partition_t cache[64] = {0};
static int cache_size = 0;

int is_partitioned(uint8_t disk) {
    return has_gpt(disk) || has_mbr(disk);
}

int get_partition(uint8_t disk, uint8_t partition, partition_t *out) {
    // Check cache
    for (int i = 0; i < cache_size; i++) {
        partition_t* entry = &cache[i];
        if (entry->disk == disk && entry->partition == partition) {
            *out = *entry;
            return 0;
        }
    }
    if (has_mbr(disk) > 0) {
        int res = mbr_get_partition(disk, partition, out);
        if (res < 0) return res;
        if (cache_size < 64) {
            cache[cache_size++] = *out;
        }
        return res;
    } else if (has_gpt(disk)) {
        int res = gpt_get_partition(disk, partition, out);
        if (res < 0) return res;
        if (cache_size < 64) {
            cache[cache_size++] = *out;
        }
        return res;
    } else {
        return -ENODEV;
    }
}

int read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    partition_t part;
    int p = get_partition(disk, partition, &part);
    if (p < 0) {
        return p; // Partition not found or read error
    }
    uint64_t adjusted_lba = part.start + lba;
    if (adjusted_lba < part.start ||
        adjusted_lba + count > part.start + part.size) {
        return -EINVAL; // Out of bounds
    }
    return read_sectors(disk, adjusted_lba, buffer, count);
}

int write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    partition_t part;
    int p = get_partition(disk, partition, &part);
    if (p < 0) {
        return p; // Partition not found or read error
    }
    // Adjust LBA to be relative to the partition start
    uint64_t adjusted_lba = part.start + lba;
    if (adjusted_lba < part.start ||
        adjusted_lba + count > part.start + part.size) {
        return -EINVAL; // Out of bounds
    }
    return write_sectors(disk, adjusted_lba, buffer, count);
}
