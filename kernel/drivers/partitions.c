#include "partitions.h"
#include "block.h"
#include "../error.h"
#include "../memory/mman.h"
#include <stdint.h>

partition_t partitions[256] = {0};
int partition_count = 0;
int partition_driver_index = 0;

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

static char* strncpy(char *dest, const char *src, size_t n) {
    char* orig_dest = dest;
    int s = 0;
    while (s < n && src[s] != 0) {
        dest[s] = src[s];
        s++;
    }
    if (s < n) dest[s] = '\0';
    return dest;
}

static int digits_u64(uint64_t v) {
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

static void itoa(uint64_t value, char* buffer) {
    int len = digits_u64(value);
    buffer[len] = '\0';

    if (value == 0) {
        buffer[0] = '0';
        return;
    }

    while (len--) {
        buffer[len] = '0' + (value % 10);
        value /= 10;
    }
}

int has_mbr(block_device_t device) {
    uint8_t buffer[1024];
    int res = read_sectors(device, 0, buffer, 2);
    if (res < 0) return res;
    uint8_t mbr_signature = memcmp(&buffer[510], (char[]){0x55, 0xAA}, 2);
    uint8_t gpt_signature = memcmp(&buffer[512], "EFI PART", 8);
    if (gpt_signature == 0) return 0;
    if (mbr_signature == 0) return 1;
    return 0;
}

int mbr_detect_partitions(block_device_t device) {
    if (!has_mbr(device)) return 0;
    uint8_t buffer[512];
    int res = read_sectors(device, 0, buffer, 1);
    if (res < 0) return res;

    int found = 0;
    for (int i = 0; i < 4; i++) {
        mbr_partition_entry_t* part = (void*)(buffer + 446 + i * 16);
        if (part->partition_type == 0x00) continue; // Empty
        if (part->partition_type == 0x05 || part->partition_type == 0x0F) continue; // Extended partition
        found++;
        partition_t output;
        output.blockdev = device;
        output.start = part->start_lba;
        output.size = part->size_in_sectors;
        partitions[partition_count++] = output;
        block_device_t output_device;
        output_device.is_partition = 1;
        output_device.major_number = partition_driver_index;
        output_device.minor_number = partition_count - 1;
        output_device.name = kmalloc(256);
        strncpy(output_device.name, device.name, 244);
        if (output_device.name[strlen(output_device.name) - 1] >= '0' &&
        output_device.name[strlen(output_device.name) - 1] <= '9')
        output_device.name[strlen(output_device.name)] = 'p';
        itoa(i + 1, &output_device.name[strlen(output_device.name)]);
        register_block_device(&output_device);
    }
    return found;
}

int has_gpt(block_device_t device) {
    uint8_t buffer[1024];
    int res = read_sectors(device, 0, buffer, 2);
    if (res < 0) return res;
    return memcmp(&buffer[512], "EFI PART", 8) == 0;
}

int gpt_detect_partitions(block_device_t device) {
    if (!has_gpt(device)) return 0;

    uint8_t header_buffer[512];
    int res = read_sectors(device, 1, header_buffer, 1);
    if (res < 0) return res;

    gpt_header_t *header = (gpt_header_t *)header_buffer;
    if (memcmp(header->signature, "EFI PART", 8) != 0) return 0;
    if (header->size_of_partition_entry == 0 || header->num_partition_entries == 0) return 0;

    uint32_t entries_per_sector = 512 / header->size_of_partition_entry;
    if (entries_per_sector == 0) return 0;

    uint32_t total_entries = header->num_partition_entries;
    uint32_t sectors = (total_entries + entries_per_sector - 1) / entries_per_sector;
    uint8_t entry_buffer[512];
    int found = 0;

    for (uint32_t sector_index = 0; sector_index < sectors; sector_index++) {
        uint64_t lba = header->partition_entry_lba + sector_index;
        res = read_sectors(device, lba, entry_buffer, 1);
        if (res < 0) return res;

        uint32_t entries_this_sector = entries_per_sector;
        if ((sector_index + 1) * entries_per_sector > total_entries) {
            entries_this_sector = total_entries - sector_index * entries_per_sector;
        }

        for (uint32_t entry_index = 0; entry_index < entries_this_sector; entry_index++) {
            uint8_t *entry_ptr = entry_buffer + entry_index * header->size_of_partition_entry;
            gpt_entry_t *gpt_entry = (gpt_entry_t *)entry_ptr;

            int empty = 1;
            for (int i = 0; i < 16; i++) {
                if (gpt_entry->partition_type_guid[i] != 0) {
                    empty = 0;
                    break;
                }
            }
            if (empty) continue;
            if (gpt_entry->starting_lba == 0 || gpt_entry->ending_lba < gpt_entry->starting_lba) continue;
            if (partition_count >= 256) return -ENOSPC;

            partition_t output = {0};
            output.blockdev = device;
            output.start = gpt_entry->starting_lba;
            output.size = gpt_entry->ending_lba - gpt_entry->starting_lba + 1;
            partitions[partition_count++] = output;

            block_device_t output_device = {0};
            output_device.is_partition = 1;
            output_device.major_number = partition_driver_index;
            output_device.minor_number = partition_count - 1;
            output_device.name = kmalloc(256);
            strncpy(output_device.name, device.name, 244);
            if (output_device.name[strlen(output_device.name) - 1] >= '0' &&
            output_device.name[strlen(output_device.name) - 1] <= '9')
            output_device.name[strlen(output_device.name)] = 'p';
            itoa(found + 1, &output_device.name[strlen(output_device.name)]);
            register_block_device(&output_device);
            found++;
        }
    }

    return found;
}

int detect_partitions(block_device_t device) {
    if (has_gpt(device)) {
        return gpt_detect_partitions(device);
    } else if (has_mbr(device)) {
        return mbr_detect_partitions(device);
    } else {
        return 0;
    }
}

int partition_read_sectors(int partition, uint64_t lba, uint8_t* buffer, uint64_t count) {
    partition_t part = partitions[partition];
    uint64_t adjusted_lba = part.start + lba;
    if (adjusted_lba < part.start ||
        adjusted_lba + count > part.start + part.size) {
        return -EINVAL; // Out of bounds
    }
    return read_sectors(part.blockdev, adjusted_lba, buffer, count);
}

int partition_write_sectors(int partition, uint64_t lba, const uint8_t* buffer, uint64_t count) {
    partition_t part = partitions[partition];
    uint64_t adjusted_lba = part.start + lba;
    if (adjusted_lba < part.start ||
        adjusted_lba + count > part.start + part.size) {
        return -EINVAL; // Out of bounds
    }
    return write_sectors(part.blockdev, adjusted_lba, buffer, count);
}

int64_t partition_get_sector_size(int partition) {
    return get_sector_size(partitions[partition].blockdev);
}

int64_t partition_get_size(int partition) {
    return partitions[partition].size * partition_get_sector_size(partition);
}

void partition_init() {
    block_driver_t driver = {
        .read_sectors = partition_read_sectors,
        .write_sectors = partition_write_sectors,
        .get_blockdev_size = partition_get_size,
        .get_sector_size = partition_get_sector_size,
    };
    partition_driver_index = register_block_driver(&driver);
    if (partition_driver_index < 0) partition_driver_index = 0; // Error, disable driver
}
