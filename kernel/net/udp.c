#include "udp.h"
#include "ip.h"
#include "ethernet.h"
#include "../error.h"
#include <stdint.h>
#include "../memory/mman.h"
#include "socket.h"

uint8_t udp_packet_buffer[UDP_BUFFER_SIZE][1484];

uint8_t udp_ports_open[65536];

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

int udp_send(uint8_t *dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t *data, int data_len) {
    uint8_t ip[4];
    *(uint32_t*)ip = get_source_ip_for(dest_ip);

    if (data_len > UDP_MAX_DATA_SIZE) return -EMSGSIZE;

    // Allocate memory for UDP packet
    int udp_packet_size = UDP_HEADER_SIZE + data_len;
    uint8_t* udp_packet = kmalloc(udp_packet_size);

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
    int res = ip_send(dest_ip, 17, udp_packet, udp_packet_size); // 17 is the protocol number for UDP
    kfree(udp_packet);
    return res;
}

void udp_received(uint8_t *packet, uint8_t *sender, uint8_t *dest, int len) {
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

    uint16_t dest_port = ntohs(udp_header->dest_port);
    if (udp_ports_open[dest_port]) {
        for (int i = 0; i < UDP_BUFFER_SIZE; i++) {
            if (((udp_header_t*)(udp_packet_buffer[i] + 4))->length == 0) {
                memcpy(udp_packet_buffer[i], sender, 4);
                memcpy(udp_packet_buffer[i] + 4, packet, len);
                break;
            }
        }
    } else {
        ip_send_dest_unreachable(sender, 3);
    }
}

int64_t udp_get_packet(uint16_t port, uint8_t* payload, size_t len, sockaddr_in_t* sender, int peek) {
    for (int i = 0; i < UDP_BUFFER_SIZE; i++) {
        uint8_t* buf = udp_packet_buffer[i];
        udp_header_t* header = (udp_header_t*)(buf + 4);
        if (header->length == 0) continue;
        if (ntohs(header->dest_port) != port) continue;
        size_t size = header->length - sizeof(udp_header_t);
        if (size > len) size = len;
        memcpy(payload, buf + 4 + 8, size);
        sender->type = AF_INET;
        memcpy(sender->ip, buf, 4);
        sender->port = header->src_port;
        if (!peek) header->length = 0;
        return size;
    }
    return -EAGAIN;
}

int udp_open(uint16_t port_number) {
    if (udp_ports_open[port_number]) return -EBUSY;
    udp_ports_open[port_number] = 1;
    return 0;
}

void udp_close(uint16_t port_number) {
    udp_ports_open[port_number] = 0;
}
