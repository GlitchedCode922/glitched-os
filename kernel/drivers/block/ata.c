#include "ata.h"
#include "../../io/ports.h"
#include <stdint.h>

ata_device_t devices[4] = {0};

ata_device_t detect_packet_device(uint16_t bus_port, uint16_t disk) {
    return (ata_device_t){0}; // Placeholder, wwill be added in ATAPI driver
}

void scan_for_devices() {
    devices[0] = detect_device(PRIMARY_BUS, 0xA0);
    devices[1] = detect_device(PRIMARY_BUS, 0xB0);
    devices[2] = detect_device(SECONDARY_BUS, 0xA0);
    devices[3] = detect_device(SECONDARY_BUS, 0xB0);
}

ata_device_t detect_device(uint16_t bus_port, uint16_t disk) {
    // Check if the device exists
    ata_device_t device = {0};
    if (inb(bus_port + 0x07) == 0xFF) {
        device.exists = 0;
        return device;
    }

    select_drive(bus_port, disk);

    outb(bus_port + 0x02, 0x00);
    outb(bus_port + 0x03, 0x00);
    outb(bus_port + 0x04, 0x00);
    outb(bus_port + 0x05, 0x00);

    outb(bus_port + 0x07, 0xEC);
    uint8_t status = inb(bus_port + 0x07);
    if (status == 0) {
        device.exists = 0;
        return device;
    }

    if (inb(bus_port + 0x01) != 0) {
         return detect_packet_device(bus_port, disk);
    }

    while (inb(bus_port + 0x07) & 0x80);

    uint8_t lbamid = inb(bus_port + 0x04);
    uint8_t lbahigh = inb(bus_port + 0x05);

    if (lbamid != 0 && lbahigh != 0) {
        return detect_packet_device(bus_port, disk);
    }
    while (!(inb(bus_port + 0x07) & 9));

    device.exists = 1;
    device.type = 0;

    if (!(inb(bus_port + 0x07) & 1)) {
        for (int i=0;i<256;i++) {
            device.identify[i] = inw(bus_port);
        }
    }
    return device;
}

void select_drive(uint16_t bus_port, uint16_t disk) {
    outb(bus_port + 0x06, disk);
    for (int i = 0; i < 5; i++) {
        inb(bus_port + 0x206); // Read alternate status register 5 times to create a 400ns delay
    }
}

int read_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count) {
    // Select the drive
    if (devices[drive].exists == 0) return -2;

    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);

    // Enable LBA
    outb(bus_port + 6, 0xE0 | ((drive % 2) << 4));

    if (supports_lba48(drive)) {
        outb(bus_port + 2, (count >> 8) & 0xFF);  // sector count high
        outb(bus_port + 3, (lba >> 24) & 0xFF);   // LBA low high
        outb(bus_port + 4, (lba >> 32) & 0xFF);   // LBA mid high
        outb(bus_port + 5, (lba >> 40) & 0xFF);   // LBA high high

        outb(bus_port + 2, count & 0xFF);         // sector count low
        outb(bus_port + 3, lba & 0xFF);           // LBA low low
        outb(bus_port + 4, (lba >> 8) & 0xFF);    // LBA mid low
        outb(bus_port + 5, (lba >> 16) & 0xFF);   // LBA high low

        // Send extended read sectors command (0x24)
        outb(bus_port + 7, 0x24);
    } else if (count < 256 && lba + count < 0xFFFFFFF) {
        // Set sector count
        outb(bus_port + 2, count);

        // Set LBA low, mid, high bytes
        outb(bus_port + 3, (uint8_t)(lba & 0xFF));
        outb(bus_port + 4, (uint8_t)((lba >> 8) & 0xFF));
        outb(bus_port + 5, (uint8_t)((lba >> 16) & 0xFF));

        // Send read sectors command (0x20)
        outb(bus_port + 7, 0x20);
    } else {
        return -3; // Invalid LBA or sector count for non-LBA48 devices
    }

    char to_retry[count];

    for (uint8_t sector = 0; sector < count; sector++) {
        to_retry[sector] = 0;

        // Wait for BSY to clear and DRQ to set
        while (inb(bus_port + 7) & 0x80);       // Wait while BSY (busy) is set
        while (!(inb(bus_port + 7) & 0x09));    // Wait for DRQ (data request) or ERR set

        if (inb(bus_port + 7) & 0x01) {
            // Hardware error
            to_retry[sector] = 1;
        }

        // Read 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            ((uint16_t *)buffer)[sector * 256 + i] = inw(bus_port);
        }
    }
    // Retry reading sectors that had errors
    for (uint8_t sector = 0; sector < count; sector++) {
        if (to_retry[sector]) {
            // Retry reading the sector
            outb(bus_port + 2, 1); // Set sector count to 1

            uint32_t retry_lba = lba + sector;

            if (supports_lba48(drive)) {
                outb(bus_port + 2, (count >> 8) & 0xFF);  // sector count high
                outb(bus_port + 3, (lba >> 24) & 0xFF);   // LBA low high
                outb(bus_port + 4, (lba >> 32) & 0xFF);   // LBA mid high
                outb(bus_port + 5, (lba >> 40) & 0xFF);   // LBA high high

                outb(bus_port + 2, count & 0xFF);         // sector count low
                outb(bus_port + 3, lba & 0xFF);           // LBA low low
                outb(bus_port + 4, (lba >> 8) & 0xFF);    // LBA mid low
                outb(bus_port + 5, (lba >> 16) & 0xFF);   // LBA high low

                // Send extended read sectors command (0x24)
                outb(bus_port + 7, 0x24);
            } else if (count < 256 && retry_lba < 0xFFFFFFF) {
                // Set sector count
                outb(bus_port + 2, count);

                // Set LBA low, mid, high bytes
                outb(bus_port + 3, (uint8_t)(lba & 0xFF));
                outb(bus_port + 4, (uint8_t)((lba >> 8) & 0xFF));
                outb(bus_port + 5, (uint8_t)((lba >> 16) & 0xFF));

                // Send read sectors command (0x20)
                outb(bus_port + 7, 0x20);
            }

            while (inb(bus_port + 7) & 0x80); // Wait for BSY to clear
            while (!(inb(bus_port + 7) & 0x09)); // Wait for DRQ or ERR

            if (inb(bus_port + 7) & 0x01) {
                return -1; // Hardware error on retry
            }

            // Read the sector again
            for (int i = 0; i < 256; i++) {
                ((uint16_t *)buffer)[sector * 256 + i] = inw(bus_port);
            }
        }
    }
    return 0;
}

int write_sectors(uint8_t drive, uint64_t lba, uint8_t *buffer, uint16_t count) {
    // Select the drive
    if (devices[drive].exists == 0) return -2;

    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);

    // Enable LBA
    outb(bus_port + 6, 0xE0 | ((drive % 2) << 4));

    if (supports_lba48(drive)) {
        outb(bus_port + 2, (count >> 8) & 0xFF);  // sector count high
        outb(bus_port + 3, (lba >> 24) & 0xFF);   // LBA low high
        outb(bus_port + 4, (lba >> 32) & 0xFF);   // LBA mid high
        outb(bus_port + 5, (lba >> 40) & 0xFF);   // LBA high high

        outb(bus_port + 2, count & 0xFF);         // sector count low
        outb(bus_port + 3, lba & 0xFF);           // LBA low low
        outb(bus_port + 4, (lba >> 8) & 0xFF);    // LBA mid low
        outb(bus_port + 5, (lba >> 16) & 0xFF);   // LBA high low

        // Send extended write sectors command (0x34)
        outb(bus_port + 7, 0x34);
    } else if (count < 256 && lba + count < 0xFFFFFFF) {
        // Set sector count
        outb(bus_port + 2, count);

        // Set LBA low, mid, high bytes
        outb(bus_port + 3, (uint8_t)(lba & 0xFF));
        outb(bus_port + 4, (uint8_t)((lba >> 8) & 0xFF));
        outb(bus_port + 5, (uint8_t)((lba >> 16) & 0xFF));

        // Send write sectors command (0x30)
        outb(bus_port + 7, 0x30);
    } else {
        return -3; // Invalid LBA or sector count for non-LBA48 devices
    }

    char to_retry[count];

    for (uint8_t sector = 0; sector < count; sector++) {
        to_retry[sector] = 0;

        // Wait for BSY to clear and DRQ to set
        while (inb(bus_port + 7) & 0x80);       // Wait while BSY (busy) is set
        while (!(inb(bus_port + 7) & 0x09));    // Wait for DRQ (data request) or ERR set

        if (inb(bus_port + 7) & 0x01) {
            // Hardware error
            to_retry[sector] = 1;
        }

        // Write 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            outw(bus_port, ((uint16_t *)buffer)[sector * 256 + i]);
        }
    }
    // Retry writing sectors that had errors
    for (uint8_t sector = 0; sector < count; sector++) {
        if (to_retry[sector]) {
            // Retry writing the sector

            outb(bus_port + 2, 1); // Set sector count to 1

            uint32_t retry_lba = lba + sector;
            if (supports_lba48(drive)) {
                outb(bus_port + 2, 0);                    // sector count high
                outb(bus_port + 3, (lba >> 24) & 0xFF);   // LBA low high
                outb(bus_port + 4, (lba >> 32) & 0xFF);   // LBA mid high
                outb(bus_port + 5, (lba >> 40) & 0xFF);   // LBA high high

                outb(bus_port + 2, 1);                    // sector count low
                outb(bus_port + 3, lba & 0xFF);           // LBA low low
                outb(bus_port + 4, (lba >> 8) & 0xFF);    // LBA mid low
                outb(bus_port + 5, (lba >> 16) & 0xFF);   // LBA high low

                // Send extended write sectors command (0x34)
                outb(bus_port + 7, 0x34);
            } else if (retry_lba < 0xFFFFFFF) {
                // Set sector count
                outb(bus_port + 2, 1);

                // Set LBA low, mid, high bytes
                outb(bus_port + 3, (uint8_t)(lba & 0xFF));
                outb(bus_port + 4, (uint8_t)((lba >> 8) & 0xFF));
                outb(bus_port + 5, (uint8_t)((lba >> 16) & 0xFF));

                // Send write sectors command (0x30)
                outb(bus_port + 7, 0x30);
            } else {
                return -3; // Invalid LBA or sector count for non-LBA48 devices
            }
            
            while (inb(bus_port + 7) & 0x80); // Wait for BSY to clear
            while (!(inb(bus_port + 7) & 0x09)); // Wait for DRQ or ERR

            if (inb(bus_port + 7) & 0x01) {
                return -1; // Hardware error on retry
            }

            // Write the sector again
            for (int i = 0; i < 256; i++) {
                outw(bus_port, ((uint16_t *)buffer)[sector * 256 + i]);
            }
        }
    }
    return 0;
}

int get_smart_data(uint8_t drive, uint8_t* buffer) {
    if (devices[drive].exists == 0) return -2;

    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    uint8_t drive_select = (drive % 2) << 4;

    select_drive(bus_port, drive_select);

    // Wait for drive to be ready
    while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear

    // Send SMART READ DATA command sequence
    outb(bus_port + 0x01, 0xD0);                    // Feature register: SMART READ DATA subcommand
    outb(bus_port + 0x02, 0x01);                    // Sector count = 1
    outb(bus_port + 0x03, 0x00);                    // LBA Low = 0
    outb(bus_port + 0x04, 0x4F);                    // LBA Mid = 0x4F
    outb(bus_port + 0x05, 0xC2);                    // LBA High = 0xC2
    outb(bus_port + 0x06, 0xE0 | drive_select);     // Drive/head
    outb(bus_port + 0x07, 0xB0);                    // Command: SMART

    // Wait for command to complete
    while (inb(bus_port + 0x07) & 0x80);            // Wait for BSY to clear
    if (!(inb(bus_port + 0x07) & 0x08)) return -1;     // If DRQ not set, no data to read

    // Read 256 words (512 bytes) from the data port
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = inw(bus_port);     // Read word from data port
    }
    return 0;
}

int supports_lba48(int drive) {
    if (devices[drive].exists == 0) return 0;
    return (devices[drive].identify[83] & 0x0400) == 0; // Check bit 10 of word 83 in IDENTIFY data
}

uint64_t get_drive_size(int drive) {
    if (devices[drive].exists == 0) return 0;
    uint64_t size = 0;
    if (supports_lba48(drive)) {
        size = ((uint64_t)devices[drive].identify[103] << 48) | ((uint64_t)devices[drive].identify[102] << 32) | ((uint64_t)devices[drive].identify[101] << 16) | devices[drive].identify[100];
    } else {
        size = ((uint32_t)devices[drive].identify[61] << 16) | (uint32_t)(devices[drive].identify[60]);
    }
    return size;
}

void standby(int drive) {
    if (devices[drive].exists == 0) return;
    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);
    outb(bus_port + 0x07, 0xE2);
}
