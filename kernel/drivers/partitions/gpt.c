#include "gpt.h"
#include <stdint.h>
#include "../block.h"
#include "../../console.h"
#include "../../memory/mman.h"

int has_gpt(uint8_t disk) {
    uint8_t buffer[1024];
    if (read_sectors(disk, 0, buffer, 2) != 0) return 0;
    return memcmp(&buffer[512], "EFI PART", 8) == 0;
}

uint64_t gpt_get_partition_start(uint8_t disk, uint8_t partition) {
    if (!has_gpt(disk)) {
        return 0; // Not a GPT disk
    }
    uint8_t gpt_header_buffer[512];
    if (read_sectors(disk, 1, gpt_header_buffer, 1) != 0) {
        return 0; // Handle read error
    }
    gpt_header_t *gpt_header = (gpt_header_t *)gpt_header_buffer;
    uint32_t partition_entry_lba = gpt_header->partition_entry_lba;
    uint32_t size_of_partition_entry = gpt_header->size_of_partition_entry;

    uint8_t partition_entry_buffer[512];
    uint32_t entries_per_sector = 512 / size_of_partition_entry;
    uint32_t sector_offset = (partition * size_of_partition_entry) / 512;
    uint32_t entry_offset = (partition * size_of_partition_entry) % 512;

    if (read_sectors(disk, partition_entry_lba + sector_offset, partition_entry_buffer, 1) != 0) {
        return 0; // Handle read error
    }

    gpt_entry_t *entry = (gpt_entry_t *)(partition_entry_buffer + entry_offset);
    return entry->starting_lba;
}

uint64_t gpt_get_partition_size(uint8_t disk, uint8_t partition) {
    if (!has_gpt(disk)) {
        return 0; // Not a GPT disk
    }
    uint8_t gpt_header_buffer[512];
    if (read_sectors(disk, 1, gpt_header_buffer, 1) != 0) {
        return 0; // Handle read error
    }
    gpt_header_t *gpt_header = (gpt_header_t *)gpt_header_buffer;
    uint32_t partition_entry_lba = gpt_header->partition_entry_lba;
    uint32_t size_of_partition_entry = gpt_header->size_of_partition_entry;

    uint8_t partition_entry_buffer[512];
    uint32_t entries_per_sector = 512 / size_of_partition_entry;
    uint32_t sector_offset = (partition * size_of_partition_entry) / 512;
    uint32_t entry_offset = (partition * size_of_partition_entry) % 512;

    if (read_sectors(disk, partition_entry_lba + sector_offset, partition_entry_buffer, 1) != 0) {
        return 0; // Handle read error
    }

    gpt_entry_t *entry = (gpt_entry_t *)(partition_entry_buffer + entry_offset);
    return entry->ending_lba - entry->starting_lba + 1;
}

int gpt_read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (!has_gpt(disk)) {
        return -1; // Not a GPT disk
    }
    uint64_t partition_start = gpt_get_partition_start(disk, partition);
    if (partition_start == 0) {
        return -2; // Partition not found or read error
    }
    // Adjust LBA to be relative to the partition start
    uint64_t adjusted_lba = partition_start + lba;
    if (adjusted_lba < partition_start ||
        adjusted_lba + count > partition_start + gpt_get_partition_size(disk, partition)) {
        return -3; // Out of bounds
    }
    return read_sectors(disk, adjusted_lba, buffer, count);
}

int gpt_write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (!has_gpt(disk)) {
        return -1; // Not a GPT disk
    }
    uint64_t partition_start = gpt_get_partition_start(disk, partition);
    if (partition_start == 0) {
        return -2; // Partition not found or read error
    }
    // Adjust LBA to be relative to the partition start
    uint64_t adjusted_lba = partition_start + lba;
    if (adjusted_lba < partition_start ||
        adjusted_lba + count > partition_start + gpt_get_partition_size(disk, partition)) {
        return -3; // Out of bounds
    }
    return write_sectors(disk, adjusted_lba, buffer, count);
}
