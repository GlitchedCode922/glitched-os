#include "rtl8139.h"
#include "../../io/pci.h"
#include "../../io/ports.h"
#include "../../memory/mman.h"
#include "../../memory/paging.h"
#include "../../console.h"
#include <stddef.h>
#include <stdint.h>

static uint16_t base_io_addresses[4] = {0};
static uint8_t irqs[4] = {0};
static uint8_t* rx_buffers[4] = {0};
static size_t rx_offsets[4] = {0};
static void* received_packets[4][512] = {0};
static int received_packet_lengths[4][512] = {0};
static int received_packet_read_head[4] = {0};
static int received_packet_write_head[4] = {0};
int cards_existing = 0;

void rtl8139_init(pci_device_t device) {
    if (cards_existing == 4) return;

    // Initialize the RTL8139 network card
    uint32_t bar0 = device.bar[0] & ~0x3; // Masking to get the base address
    base_io_addresses[cards_existing] = bar0 & 0xFFFF;
    irqs[cards_existing] = device.irq;
    uint32_t command = pci_config_read(device.bus, device.device, device.function, 0x04);
    command |= 0x07; // Set the bus mastering bit
    pci_config_write(device.bus, device.device, device.function, 0x04, command);
    rx_buffers[cards_existing] = kmalloc(8192 + 16); // Allocate buffer for receiving packets
    for (int i = 0; i < 512; i++) received_packets[cards_existing][i] = kmalloc(2048);
    turn_on_rtl8139(cards_existing);
    cards_existing++;
}

void turn_on_rtl8139(int card) {
    if (card > cards_existing) return; // Invalid card index

    uint16_t io_base = base_io_addresses[card];

    // Enable the RTL8139
    outb(io_base + 0x52, 0x00);

    // Reset the RTL8139
    outb(io_base + 0x37, 0x10); // Issue a reset command
    while ((inb(io_base + 0x37) & 0x10)); // Wait for reset to complete

    // Interrupt setup
    outw(io_base + 0x3C, 0x0005); // Set the TOK and ROK bits high

    // Receive buffer setup
    uint32_t rx_buffer_phys = (uint32_t)get_physical_address((uint64_t)rx_buffers[card]);
    outl(io_base + 0x30, rx_buffer_phys); // Set the receive buffer address

    // Configure the receive mode (broadcast, multicast, physical match)
    outl(io_base + 0x44, 0x0000000E);

    // Enable the receiver and transmitter
    outb(io_base + 0x37, 0x0C); // Enable receiver and transmitter
}

void rtl8139_irq_handler(uint8_t irq) {
    // Handle the interrupt for the RTL8139 network card
    for (int i = 0; i < cards_existing; i++) {
        if (irqs[i] == irq) {
            uint16_t io_base = base_io_addresses[i];
            uint16_t status = inw(io_base + 0x3E); // Read the interrupt status
            // Acknowledge the interrupt
            outw(io_base + 0x3E, status);

            // Check for received packets
            if (status & 0x01) {
                uint8_t* rx_buffer = rx_buffers[i];
                size_t offset = rx_offsets[i];

                uint16_t packet_status = *(uint16_t*)(rx_buffer + offset);
                uint16_t packet_length = *(uint16_t*)(rx_buffer + offset + 2);

                if (!(packet_status & 0x01)) {
                    // Skipping bad packet
                    offset = (offset + packet_length + 4 + 3) & ~3; // Align to 4 bytes
                    rx_offsets[i] = offset % 8192; // Wrap around the buffer
                    uint16_t capr = (rx_offsets[i] >= 16) ? rx_offsets[i] - 16 : 8192 + rx_offsets[i] - 16;
                    outw(io_base + 0x38, capr);
                    break;
                }

                // Copy the packet
                void* packet = received_packets[i][received_packet_write_head[i]];
                uint8_t* frame = rx_buffer + offset + 4;
                for (int j = 0; j < packet_length; j++) {
                    ((uint8_t*)packet)[j] = *frame;
                    frame++;
                    // Wrap check
                    if ((offset + 4 + j) >= 8192) frame = rx_buffer + ((offset + 4 + j) % 8192);
                }

                received_packet_lengths[i][received_packet_write_head[i]] = packet_length;

                received_packet_write_head[i] = (received_packet_write_head[i] + 1) % 512;

                // Inform network stack (not implemented here)

                offset = (offset + packet_length + 4 + 3) & ~3; // Align to 4 bytes
                rx_offsets[i] = offset % 8192; // Wrap around the buffer
                uint16_t capr = (rx_offsets[i] >= 16) ? rx_offsets[i] - 16 : 8192 + rx_offsets[i] - 16;
                outw(io_base + 0x38, capr);
            }

            break;
        }
    }
}

uint8_t* rtl8139_get_mac_address(int card) {
    static uint8_t mac[6] = {0};

    if (card >= cards_existing) {
        for (int i = 0; i < 6; i++) mac[i] = 0;
        return mac; // Invalid card index
    }

    uint16_t io_base = base_io_addresses[card];
    for (int i = 0; i < 6; i++) {
        mac[i] = inb(io_base + i);
    }

    return mac;
}

int rtl8139_read_packet(int card, void** buffer) {
    if (card >= cards_existing) return -1; // Invalid card index
    if (received_packet_read_head[card] == received_packet_write_head[card]) {
        // No packets available
        return -1;
    }

    void* packet = received_packets[card][received_packet_read_head[card]];
    *buffer = packet;

    int length = received_packet_lengths[card][received_packet_read_head[card]];

    received_packet_read_head[card] = (received_packet_read_head[card] + 1) % 512;
    return length;
}

void register_rtl8139_driver() {
    pci_driver_t rtl8139_driver = {
        .vendor = 0x10EC, // Realtek
        .device = 0x8139, // RTL8139
        .init = rtl8139_init,
        .isr = rtl8139_irq_handler,
    };
    register_pci_driver(rtl8139_driver);
}
