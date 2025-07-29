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

void read_sectors(uint8_t drive, uint32_t lba, uint8_t *buffer, uint8_t count) {
    // Select the drive
    if (devices[drive].exists == 0) return;

    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);

    // Enable LBA
    outb(bus_port + 6, 0xE0 | ((drive % 2) << 4));

    // Set sector count
    outb(bus_port + 2, count);

    // Set LBA low, mid, high bytes
    outb(bus_port + 3, (uint8_t)(lba & 0xFF));
    outb(bus_port + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(bus_port + 5, (uint8_t)((lba >> 16) & 0xFF));

    // Send read sectors command (0x20)
    outb(bus_port + 7, 0x20);

    for (uint8_t sector = 0; sector < count; sector++) {
        // Wait for BSY to clear and DRQ to set
        while (inb(bus_port + 7) & 0x80);       // Wait while BSY (busy) is set
        while (!(inb(bus_port + 7) & 0x08));    // Wait for DRQ (data request) set

        // Read 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            ((uint16_t *)buffer)[sector * 256 + i] = inw(bus_port);
        }
    }
}

void write_sectors(uint8_t drive, uint32_t lba, uint8_t *buffer, uint8_t count) {
    // Select the drive
    if (devices[drive].exists == 0) return;

    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);

    // Enable LBA
    outb(bus_port + 6, 0xE0 | ((drive % 2) << 4));

    // Set sector count
    outb(bus_port + 2, count);

    // Set LBA low, mid, high bytes
    outb(bus_port + 3, (uint8_t)(lba & 0xFF));
    outb(bus_port + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(bus_port + 5, (uint8_t)((lba >> 16) & 0xFF));

    // Send write sectors command (0x30)
    outb(bus_port + 7, 0x30);

    for (uint8_t sector = 0; sector < count; sector++) {
        // Wait for BSY to clear and DRQ to set
        while (inb(bus_port + 7) & 0x80);       // Wait while BSY (busy) is set
        while (!(inb(bus_port + 7) & 0x08));    // Wait for DRQ (data request) set

        // Write 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            outw(bus_port, ((uint16_t *)buffer)[sector * 256 + i]);
        }
    }
}
