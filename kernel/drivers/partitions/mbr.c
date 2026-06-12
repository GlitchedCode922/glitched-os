#include "mbr.h"
#include "../../memory/mman.h"
#include "../block.h"
#include "../../console.h"
#include "../../error.h"
#include <stdint.h>
#include <stddef.h>

int has_mbr(uint8_t disk) {
    uint8_t buffer[1024];
    int res = read_sectors(disk, 0, buffer, 2);
    if (res < 0) return res;
    uint8_t mbr_signature = memcmp(&buffer[510], (char[]){0x55, 0xAA}, 2);
    uint8_t gpt_signature = memcmp(&buffer[512], "EFI PART", 8);
    if (gpt_signature == 0) return 0;
    if (mbr_signature == 0) return 1;
    return 0;
}

int mbr_get_partition(uint8_t disk, uint8_t partition, partition_t* out) {
    if (partition > 4) {
        return -ENODEV; // Invalid partition number
    }
    uint8_t mbr[512] = {0};
    int res = read_sectors(disk, 0, mbr, 1);
    if (res < 0) return res;
    int partition_offset = 446 + partition * 16; // Each partition entry is 16 bytes
    mbr_partition_entry_t *entry = (mbr_partition_entry_t *)(mbr + partition_offset);

    out->disk = disk;
    out->partition = partition;
    out->start = entry->start_lba;
    out->size = entry->size_in_sectors;
    return 0;
}
