#pragma once
#include <stdint.h>

typedef struct {
    char signature[8];          // "EFI PART"
    uint32_t revision;          // GPT revision
    uint32_t header_size;       // Size of the GPT header
    uint32_t header_crc32;      // CRC32 of the GPT header
    uint32_t reserved;          // Reserved, must be zero
    uint64_t current_lba;      // Current LBA of the GPT header
    uint64_t backup_lba;       // Backup LBA of the GPT header
    uint64_t first_usable_lba; // First usable LBA for partitions
    uint64_t last_usable_lba;  // Last usable LBA for partitions
    uint8_t disk_guid[16];     // Disk GUID
    uint32_t partition_entry_lba; // LBA of the first partition entry
    uint32_t num_partition_entries; // Number of partition entries
    uint32_t size_of_partition_entry; // Size of each partition entry
    uint32_t partition_entry_array_crc32; // CRC32 of the partition entry array
} gpt_header_t;

typedef struct {
    uint8_t partition_type_guid[16]; // Partition type GUID
    uint8_t unique_partition_guid[16]; // Unique partition GUID
    uint64_t starting_lba;           // Starting LBA of the partition
    uint64_t ending_lba;             // Ending LBA of the partition
    uint64_t attributes;             // Partition attributes
    uint16_t partition_name[36];     // Partition name (UTF-16)
} gpt_entry_t;

int has_gpt(uint8_t disk);
uint64_t gpt_get_partition_start(uint8_t disk, uint8_t partition);
uint64_t gpt_get_partition_size(uint8_t disk, uint8_t partition);
int gpt_read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
int gpt_write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
