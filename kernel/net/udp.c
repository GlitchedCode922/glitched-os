#include "udp.h"
#include "ip.h"
#include "ethernet.h"
#include <stdint.h>
#include "../memory/mman.h"
#include "icmp.h"
#include "../drivers/net.h"

void (*port_listeners[65536])(uint8_t*, uint16_t, uint8_t*, int) = {0};

// Compute the UDP checksum
uint16_t udp_checksum(uint8_t* src_ip, uint8_t* dest_ip, uint8_t* udp_packet, int len) {
    // Pseudo-header for checksum calculation
    struct {
        uint8_t src_ip[4];
        uint8_t dest_ip[4];
        uint8_t zero;
        uint8_t protocol;
        uint16_t udp_length;
    } pseudo_header;

    // Fill pseudo-header
    for (int i = 0; i < 4; i++) {
        pseudo_header.src_ip[i] = src_ip[i];
        pseudo_header.dest_ip[i] = dest_ip[i];
    }
    pseudo_header.zero = 0;
    pseudo_header.protocol = 17; // UDP protocol number
    pseudo_header.udp_length = htons(len);

    // Calculate checksum over pseudo-header and UDP packet
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)&pseudo_header;
    for (int i = 0; i < sizeof(pseudo_header) / 2; i++) {
        sum += ntohs(ptr[i]);
    }

    ptr = (uint16_t*)udp_packet;
    for (int i = 0; i < (len + 1) / 2; i++) {
        sum += ntohs(ptr[i]);
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return htons(~sum);
}

void udp_send(uint8_t *dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t *data, int data_len, int card) {
    uint8_t ip[4];
    get_ip(card, (uint32_t*)ip);

    if (data_len > UDP_MAX_DATA_SIZE) {
        // Data too large for a single UDP packet
        return;
    }

    // Allocate memory for UDP packet
    int udp_packet_size = UDP_HEADER_SIZE + data_len;
    uint8_t* udp_packet = kmalloc(udp_packet_size);
    if (!udp_packet) {
        return; // Memory allocation failed
    }

    // Fill UDP header
    udp_header_t* udp_header = (udp_header_t*)udp_packet;
    udp_header->src_port = htons(src_port);
    udp_header->dest_port = htons(dest_port);
    udp_header->length = htons(udp_packet_size);
    udp_header->checksum = 0; // Initial checksum value

    // Copy data into UDP packet
    memcpy(udp_packet + UDP_HEADER_SIZE, data, data_len);

    // Compute checksum
    udp_header->checksum = udp_checksum((uint8_t*)ip, dest_ip, udp_packet, udp_packet_size);

    // Send the UDP packet using IP layer
    ip_send(dest_ip, 17, udp_packet, udp_packet_size, card); // 17 is the protocol number for UDP

    // Free allocated memory
    kfree(udp_packet);
}

void udp_received(uint8_t *packet, uint8_t *sender, uint8_t *dest, int len, int card) {
    if (len < UDP_HEADER_SIZE) {
        return; // Packet too short to be valid UDP
    }

    udp_header_t* udp_header = (udp_header_t*)packet;
    int udp_length = ntohs(udp_header->length);
    if (udp_length != len) {
        return; // Length mismatch
    }

    // Verify checksum
    uint16_t received_checksum = udp_header->checksum;
    udp_header->checksum = 0; // Set to 0 for checksum calculation
    uint16_t calculated_checksum = udp_checksum(sender, (uint8_t*)dest, packet, len);
    if (received_checksum != calculated_checksum) {
        return; // Checksum mismatch
    }

    // Extract source and destination ports
    uint16_t src_port = ntohs(udp_header->src_port);
    uint16_t dest_port = ntohs(udp_header->dest_port);

    // Extract payload
    int payload_length = len - UDP_HEADER_SIZE;
    uint8_t* payload = packet + UDP_HEADER_SIZE;

    // Check for registered listener on destination port
    if (port_listeners[dest_port]) {
        port_listeners[dest_port](sender, src_port, payload, payload_length);
    } else {
        ip_send_dest_unreachable(sender, 3, card);
    }
}

void register_udp_listener(uint16_t port, void (*listener)(uint8_t*, uint16_t, uint8_t*, int)) {
    port_listeners[port] = listener;
}

void unregister_udp_listener(uint16_t port) {
    port_listeners[port] = 0;
}
