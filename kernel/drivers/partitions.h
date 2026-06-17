#pragma once
#include "block.h"
#include <stdint.h>

#define CHS_CYLINDERS 1024
#define CHS_HEADS 256
#define CHS_SECTORS 63
#define MBR_SIGNATURE 0x55AA
#define MBR_SIZE 512
#define BOOTLOADER_SIZE 446
#define PARTITION_TABLE_SIZE 64
#define PARTITION_ENTRY_SIZE 16

typedef struct {
    uint8_t boot_indicator; // 0x80 for bootable, 0x00 for non-bootable
    uint8_t start_head;     // Starting head of the partition
    uint8_t start_sector;   // Starting sector (bits 0-5) and cylinder (bits 6-7)
    uint8_t start_cylinder; // Starting cylinder (bits 0-7)
    uint8_t partition_type; // Type of the partition
    uint8_t end_head;       // Ending head of the partition
    uint8_t end_sector;     // Ending sector (bits 0-5) and cylinder (bits 6-7)
    uint8_t end_cylinder;   // Ending cylinder (bits 0-7)
    uint32_t start_lba;     // Starting LBA address of the partition
    uint32_t size_in_sectors;// Size of the partition in sectors
} __attribute__((packed)) mbr_partition_entry_t;

typedef struct {
    uint8_t bootloader[BOOTLOADER_SIZE]; // Bootloader code
    mbr_partition_entry_t partition_table[PARTITION_TABLE_SIZE / PARTITION_ENTRY_SIZE]; // Partition entries
    uint16_t signature; // MBR signature (0x55AA)
} __attribute__((packed)) mbr_t;

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
    uint64_t partition_entry_lba; // LBA of the first partition entry
    uint32_t num_partition_entries; // Number of partition entries
    uint32_t size_of_partition_entry; // Size of each partition entry
    uint32_t partition_entry_array_crc32; // CRC32 of the partition entry array
} __attribute__((packed)) gpt_header_t;

typedef struct {
    uint8_t partition_type_guid[16]; // Partition type GUID
    uint8_t unique_partition_guid[16]; // Unique partition GUID
    uint64_t starting_lba;           // Starting LBA of the partition
    uint64_t ending_lba;             // Ending LBA of the partition
    uint64_t attributes;             // Partition attributes
    uint16_t partition_name[36];     // Partition name (UTF-16)
} __attribute__((packed)) gpt_entry_t;

typedef struct {
    block_device_t blockdev;
    int64_t start;
    int64_t size;
} partition_t;

extern partition_t partitions[256];
extern int partition_count;

int detect_partitions(block_device_t device);
void partition_init();
