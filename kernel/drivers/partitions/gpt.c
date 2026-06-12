#include "gpt.h"
#include <stdint.h>
#include "../block.h"
#include "../../error.h"
#include "../../memory/mman.h"

int has_gpt(uint8_t disk) {
    uint8_t buffer[1024];
    int res = read_sectors(disk, 0, buffer, 2);
    if (res < 0) return res;
    return memcmp(&buffer[512], "EFI PART", 8) == 0;
}

int gpt_get_partition(uint8_t disk, uint8_t partition, partition_t* out) {
    uint8_t gpt_header_buffer[512];
    int res = read_sectors(disk, 1, gpt_header_buffer, 1);
    if (res < 0) {
        return res; // Handle read error
    }
    gpt_header_t *gpt_header = (gpt_header_t *)gpt_header_buffer;
    uint32_t partition_entry_lba = gpt_header->partition_entry_lba;
    uint32_t size_of_partition_entry = gpt_header->size_of_partition_entry;

    uint8_t partition_entry_buffer[512];
    uint32_t entries_per_sector = 512 / size_of_partition_entry;
    uint32_t sector_offset = (partition * size_of_partition_entry) / 512;
    uint32_t entry_offset = (partition * size_of_partition_entry) % 512;

    if ((res = read_sectors(disk, partition_entry_lba + sector_offset, partition_entry_buffer, 1)) < 0) {
        return res; // Handle read error
    }

    // Check if the partition entry is valid (not empty)
    if (memcmp(partition_entry_buffer + entry_offset, "\0", size_of_partition_entry) == 0) {
        return -ENODEV; // Partition not found
    }

    gpt_entry_t *entry = (gpt_entry_t *)(partition_entry_buffer + entry_offset);
    out->disk = disk;
    out->partition = partition;
    out->start = entry->starting_lba;
    out->size = entry->ending_lba - entry->starting_lba + 1;
    return 0;
}
