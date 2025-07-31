#include "mbr.h"
#include "../../memory/mman.h"
#include "../block.h"
#include "../../console.h"
#include <stdint.h>
#include <stddef.h>

int has_mbr(uint8_t disk) {
    uint8_t buffer[1024];
    if (read_sectors(disk, 0, buffer, 2) != 0) return -1;
    uint8_t mbr_signature = memcmp(&buffer[510], (char[]){0x55, 0xAA}, 2);
    uint8_t gpt_signature = memcmp(&buffer[512], "EFI PART", 8);
    if (gpt_signature == 0) return 1;
    if (mbr_signature == 0) return 0;
    return 1;
}

void get_bootloader(uint8_t *buffer, uint8_t disk) {
    if (buffer == NULL) {
        return; // Handle null pointer
    }
    uint8_t mbr[512] = {0};
    if (read_sectors(disk, 0, mbr, 1) != 0) {
        return; // Handle read error
    }
    // Copy the first 446 bytes of the MBR to the buffer
    memcpy(buffer, mbr, 446);
}

void get_partition_table(uint8_t *buffer, uint8_t disk) {
    if (buffer == NULL) {
        return; // Handle null pointer
    }
    uint8_t mbr[512] = {0};
    if (read_sectors(disk, 0, mbr, 1) != 0) {
        return; // Handle read error
    }
    // Copy the partition table (bytes 446-510) to the buffer
    memcpy(buffer, mbr + 446, 64);
}

int is_bootable_disk(uint8_t disk) {
    uint8_t mbr[512] = {0};
    if (read_sectors(disk, 0, mbr, 1) != 0) {
        return -1; // Handle read error
    }
    // Check the boot signature at the end of the MBR
    return (mbr[510] == 0x55 && mbr[511] == 0xAA) ? 1 : 0;
}

int chs_to_lba(uint8_t head, uint8_t sector, uint16_t cylinder) {
    // Convert CHS to LBA
    uint32_t lba = (cylinder * 255 + head) * 63 + (sector - 1);
    return lba;
}

uint64_t get_partition_start(uint8_t disk, uint8_t partition) {
    if (partition > 4) {
        return 0; // Invalid partition number
    }
    uint8_t mbr[512] = {0};
    if (read_sectors(disk, 0, mbr, 1) != 0) {
        return 0; // Handle read error
    }
    int partition_offset = 446 + partition * 16; // Each partition entry is 16 bytes
    mbr_partition_entry_t *entry = (mbr_partition_entry_t *)(mbr + partition_offset);

    return entry->start_lba; // Return the starting LBA of the partition
}

uint64_t get_partition_size(uint8_t disk, uint8_t partition) {
    if (partition > 4) {
        return 0; // Invalid partition number
    }
    uint8_t mbr[512] = {0};
    if (read_sectors(disk, 0, mbr, 1) != 0) {
        return 0; // Handle read error
    }
    int partition_offset = 446 + partition * 16; // Each partition entry is 16 bytes
    mbr_partition_entry_t *entry = (mbr_partition_entry_t *)(mbr + partition_offset);

    return entry->size_in_sectors; // Return the size of the partition in sectors
}

int read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (partition > 4) {
        return -1; // Invalid partition number
    }
    uint64_t partition_start = get_partition_start(disk, partition);
    if (partition_start == 0) {
        return -2; // Partition not found or read error
    }
    // Adjust LBA to be relative to the partition start
    uint64_t adjusted_lba = partition_start + lba;
    if (adjusted_lba < get_partition_start(disk, partition) || 
        adjusted_lba + count > get_partition_start(disk, partition) + get_partition_size(disk, partition)) {
        return -3; // Out of bounds
    }
    return read_sectors(disk, adjusted_lba, buffer, count);
}

int write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count) {
    if (partition > 4) {
        return -1; // Invalid partition number
    }
    uint64_t partition_start = get_partition_start(disk, partition);
    if (partition_start == 0) {
        return -2; // Partition not found or read error
    }
    // Adjust LBA to be relative to the partition start
    uint64_t adjusted_lba = partition_start + lba;
    if (adjusted_lba < get_partition_start(disk, partition) || 
        adjusted_lba + count > get_partition_start(disk, partition) + get_partition_size(disk, partition)) {
        return -3; // Out of bounds
    }
    return write_sectors(disk, adjusted_lba, buffer, count);
}

void print_partition_table(uint8_t disk) {
    uint8_t mbr[512] = {0};
    if (read_sectors(disk, 0, mbr, 1) != 0) {
        kprintf("Failed to read MBR from disk %d\n", disk);
        return; // Handle read error
    }

    kprintf("Partition Table for Disk %d:\n", disk);
    for (int i = 0; i < 4; i++) {
        int offset = 446 + i * 16; // Each partition entry is 16 bytes
        mbr_partition_entry_t *entry = (mbr_partition_entry_t *)(mbr + offset);

        kprintf("Partition %d:\n", i + 1);
        kprintf("  Boot Indicator: %d\n", entry->boot_indicator);
        kprintf("  Partition Type: 0x%x\n", entry->partition_type);
        kprintf("  Start LBA: %d\n", entry->start_lba);
        kprintf("  Size in Sectors: %d\n", entry->size_in_sectors);
    }
}
