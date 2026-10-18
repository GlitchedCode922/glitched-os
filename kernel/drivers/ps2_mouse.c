#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "io/ports.h"
#include "io/8259pic.h"
#include "usermode/pipe.h"
#include "drivers/chrdev.h"
#include "fs/devfs.h"

uint8_t packet[4];
int packet_index = 0;
int supports_intellimouse = 0;
pipe_t* mouse_pipe;

int set_sample_rate(uint8_t rate) {
    outb(0x64, 0xD4);
    outb(0x60, 0xF3); // Set sample rate command
    while (!(inb(0x64) & 0x01));
    if (inb(0x60) != 0xFA) return -1; // Acknowledge check
    outb(0x64, 0xD4);
    outb(0x60, rate); // Send the sample rate
    while (!(inb(0x64) & 0x01));
    if (inb(0x60) != 0xFA) return -1; // Acknowledge check
    return 0; // Success
}

int64_t mousedev_read(int minor, uint64_t offset, uint8_t* buffer, uint64_t count) {
    return pipe_read(mouse_pipe, buffer, count);
}

int64_t mousedev_write(int minor, uint64_t offset, const uint8_t* buffer, uint64_t count) {
    return -EINVAL; // Writing to mouse device is not supported
}

int ps2_mouse_init() {
    // Try to activate Intellimouse mode
    set_sample_rate(200);
    set_sample_rate(100);
    set_sample_rate(80);
    outb(0x64, 0xD4);
    outb(0x60, 0xF2); // Identify command
    while (!(inb(0x64) & 0x01));
    if (inb(0x60) != 0xFA) return -1; // Acknowledge check
    while (!(inb(0x64) & 0x01));
    char mouse_id = inb(0x60);
    if (mouse_id == 0x03) {
        supports_intellimouse = 1;
    }
    // Enable data reporting
    outb(0x64, 0xD4);
    outb(0x60, 0xF4); // Enable data reporting command
    while (!(inb(0x64) & 0x01));
    if (inb(0x60) != 0xFA) return -1;
    pic_enable_irq(12);
    mouse_pipe = create_pipe();
    char_driver_t mouse_driver = {
        .read = mousedev_read,
        .write = mousedev_write,
    };
    int driver_id = register_char_driver(&mouse_driver);
    if (driver_id < 0) {
        return -1; // Failed to register driver
    }
    devfs_create_directory("input");
    devfs_mknod("input/mouse", DT_CHAR, makedev(driver_id, 0));
    return 0; // Success
}

int ps2_mouse_handler(uint8_t byte) {
    packet[packet_index++] = byte;
    if (!supports_intellimouse && packet_index == 3) {
        packet[packet_index++] = 0; // No fourth byte for standard mouse
    }
    if (packet_index == 4) {
        if (mouse_pipe->reader_count == 0) {
            mouse_pipe->read_head = mouse_pipe->write_head; // Discard data
            packet_index = 0; // Reset for next packet
            return 0; // No readers, discard packet
        }
        pipe_write(mouse_pipe, packet, 4, 0);
        packet_index = 0; // Reset for next packet
    }
    return 0; // Success
}
