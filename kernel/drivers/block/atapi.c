#include <stdint.h>
#include "atapi.h"
#include "ata.h"
#include "../../io/ports.h"

ata_device_t detect_packet_device(uint16_t bus_port, uint16_t disk) {
    ata_device_t device = {0};
    device.exists = 1;
    device.type = 1; // ATAPI device
    select_drive(bus_port, disk);
    outb(bus_port + 0x07, 0xA1); // Send IDENTIFY PACKET DEVICE command
    while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
    while (!(inb(bus_port + 0x07) & 0x08)); // Wait for DRQ to be set

    for (int i = 0; i < 256; i++) {
        device.identify[i] = inw(bus_port);
    }

    return device;
}

int atapi_read_sectors(uint8_t drive, uint64_t lba, uint8_t* buffer, uint32_t count) {
    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);

    // Wait for BSY to clear before issuing command
    while (inb(bus_port + 0x07) & 0x80);

    outb(bus_port + 0x04, 2048 & 0xFF);
    outb(bus_port + 0x05, 2048 >> 8);
    outb(bus_port + 0x07, 0xA0); // Send PACKET command
    
    volatile uint8_t read_cmd[12] = {0xA8, 0,
	                                 (lba >> 0x18) & 0xFF, (lba >> 0x10) & 0xFF, (lba >> 0x08) & 0xFF,
	                                 (lba >> 0x00) & 0xFF,
	                                 (count >> 0x18) & 0xFF, (count >> 0x10) & 0xFF, (count >> 0x08) & 0xFF,
	                                 (count >> 0x00) & 0xFF,
                                     0, 0};

    while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
    while (!(inb(bus_port + 0x07) & 0x09)); // Wait for DRQ or ERR to be set

    if (inb(bus_port + 0x07) & 0x01) return -1; // Error occurred

    outw(bus_port, ((uint16_t*)&read_cmd)[0]); // Send command
    outw(bus_port, ((uint16_t*)&read_cmd)[1]);
    outw(bus_port, ((uint16_t*)&read_cmd)[2]);
    outw(bus_port, ((uint16_t*)&read_cmd)[3]);
    outw(bus_port, ((uint16_t*)&read_cmd)[4]);
    outw(bus_port, ((uint16_t*)&read_cmd)[5]);

    for (int i = 0; i < count; i++) {
        while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
        while (!(inb(bus_port + 0x07) & 0x09)); // Wait for DRQ or ERR to be set

        if (inb(bus_port + 0x07) & 0x01) return -1; // Error occurred

        uint64_t size = inb(bus_port + 0x04) | (inb(bus_port + 0x05) << 8);

        for (int v = 0; v < size; v += 2) {
            uint16_t data = inw(bus_port);
            buffer[i * 2048 + v] = data & 0xFF;
            buffer[i * 2048 + v + 1] = (data >> 8) & 0xFF;
        }
    }
    return 0;
}

int atapi_write_sectors(uint8_t drive, uint64_t lba, uint8_t* buffer, uint32_t count) {
    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);

    // Wait for BSY to clear before issuing command
    while (inb(bus_port + 0x07) & 0x80);

    outb(bus_port + 0x04, 2048 & 0xFF);
    outb(bus_port + 0x05, 2048 >> 8);
    outb(bus_port + 0x07, 0xA0); // Send PACKET command
    
    volatile uint8_t write_cmd[12] = {0xAA, 0,
                                      (lba >> 0x18) & 0xFF, (lba >> 0x10) & 0xFF, (lba >> 0x08) & 0xFF,
                                      (lba >> 0x00) & 0xFF,
                                      (count >> 0x18) & 0xFF, (count >> 0x10) & 0xFF, (count >> 0x08) & 0xFF,
                                      (count >> 0x00) & 0xFF,
                                      0, 0};

    while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
    while (!(inb(bus_port + 0x07) & 0x09)); // Wait for DRQ or ERR to be set

    if (inb(bus_port + 0x07) & 0x01) return -1; // Error occurred

    outw(bus_port, ((uint16_t*)&write_cmd)[0]); // Send command
    outw(bus_port, ((uint16_t*)&write_cmd)[1]);
    outw(bus_port, ((uint16_t*)&write_cmd)[2]);
    outw(bus_port, ((uint16_t*)&write_cmd)[3]);
    outw(bus_port, ((uint16_t*)&write_cmd)[4]);
    outw(bus_port, ((uint16_t*)&write_cmd)[5]);

    for (int i = 0; i < count; i++) {
        while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
        while (!(inb(bus_port + 0x07) & 0x09)); // Wait for DRQ or ERR to be set
        if (inb(bus_port + 0x07) & 0x01) return -1; // Error occurred

        uint64_t size = inb(bus_port + 0x04) | (inb(bus_port + 0x05) << 8);

        for (int v = 0; v < size; v += 2) {
            uint16_t data = (buffer[i * 2048 + v] & 0xFF) | ((buffer[i * 2048 + v + 1] & 0xFF) << 8);
            outw(bus_port, data);
        }
    }
    return 0;
}

void atapi_load_or_eject(uint8_t drive, uint8_t load) {
    uint16_t bus_port = (drive / 2 == 0 ? PRIMARY_BUS : SECONDARY_BUS);
    select_drive(bus_port, drive % 2 == 0 ? 0xA0 : 0xB0);
    uint8_t to_load = load ? 0x01 : 0x00;
    uint8_t scsi_eject_cmd[12] = {
        0x1B,  // Operation Code: START STOP UNIT
        0x00,  // Reserved
        0x00,  // Reserved
        0x00,  // Reserved
        0x02 | to_load, // LoEj = 1 (bit 1)
        0x00,  // Control
        0x00,  // Padding
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    };

    // Wait for BSY to clear before issuing command
    while (inb(bus_port + 0x07) & 0x80);

    outb(bus_port + 0x07, 0xA0); // Send PACKET command

    while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
    while (!(inb(bus_port + 0x07) & 0x09)); // Wait for DRQ or ERR to be set

    if (inb(bus_port + 0x07) & 0x01) return; // Error occurred

    for (int i = 0; i < sizeof(scsi_eject_cmd); i += 2) {
        uint16_t data = scsi_eject_cmd[i] | (scsi_eject_cmd[i + 1] << 8);
        outw(bus_port, data); // Send command bytes
    }

    while (inb(bus_port + 0x07) & 0x80); // Wait for BSY to clear
    if (inb(bus_port + 0x07) & 0x01) return; // Error occurred
}
