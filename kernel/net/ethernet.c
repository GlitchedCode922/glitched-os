#include "../drivers/net/rtl8139.h"
#include "../memory/mman.h"
#include "ethernet.h"
#include "arp.h"
#include <stdint.h>

uint16_t htons(uint16_t val) {
    return (val >> 8) | (val << 8);
}

uint16_t ntohs(uint16_t val) {
    return (val >> 8) | (val << 8);
}

void frame_received(int card) {
    uint8_t* frame;
    int frame_length = rtl8139_read_packet(card, (void**)&frame);
    if (frame_length > 0) {
        char* src_mac = (char*)(frame + 6);
        char* dst_mac = (char*)frame;
        uint16_t ethertype = (frame[12] << 8) | frame[13];
        // Check ethertype for IPv4 (0x0800) or ARP (0x0806)
        if (ethertype == 0x0800) {
            // Handle IPv4 packet (not implemented)
        } else if (ethertype == 0x0806) {
            // Handle ARP packet
            arp_reply(frame + 14, card);
        }
    }
}

void send_ethernet(char* src_mac, char* dst_mac, uint16_t ethertype, uint8_t* payload, int payload_length, int card) {
    int frame_length = 14 + payload_length;
    if (frame_length < 60) {
        frame_length = 60; // Minimum Ethernet frame size (without CRC)
    }

    if (payload_length > 1792) return; // Exceeds MTU

    uint8_t* frame = (uint8_t*)kmalloc(frame_length);
    if (!frame) return; // Handle memory allocation failure
    // Construct Ethernet frame
    memcpy(frame, dst_mac, 6);
    memcpy(frame + 6, src_mac, 6);
    frame[12] = (ethertype >> 8) & 0xFF;
    frame[13] = ethertype & 0xFF;
    memcpy(frame + 14, payload, payload_length);
    // Send the frame using the network card
    rtl8139_send_packet(card, frame, frame_length);
    kfree(frame);
}