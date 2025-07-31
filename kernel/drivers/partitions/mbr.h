#pragma once
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

int has_mbr(uint8_t disk);
void get_bootloader(uint8_t *buffer, uint8_t disk);
void get_partition_table(uint8_t *buffer, uint8_t disk);
int is_bootable_disk(uint8_t disk);
int chs_to_lba(uint8_t head, uint8_t sector, uint16_t cylinder);
uint64_t get_partition_start(uint8_t disk, uint8_t partition);
uint64_t get_partition_size(uint8_t disk, uint8_t partition);
int read_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
int write_sectors_relative(uint8_t disk, uint8_t partition, uint64_t lba, uint8_t *buffer, uint16_t count);
void print_partition_table(uint8_t disk);

