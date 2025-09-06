#include <stdint.h>
#include "ip.h"
#include "../memory/mman.h"
#include "ethernet.h"
#include "../drivers/timer.h"
#include "icmp.h"

int pinging = 0;

// Function to calculate checksum
uint16_t icmp_checksum(uint8_t* data, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i += 2) {
        uint16_t word = data[i] << 8;
        if (i + 1 < len) {
            word |= data[i + 1];
        }
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return htons(~sum);
}

void icmp_received(uint8_t *packet, uint8_t* sender, int len, int card) {
    icmp_header_t *icmp_hdr = (icmp_header_t *)packet;

    // Verify checksum
    uint16_t received_checksum = icmp_hdr->checksum;
    icmp_hdr->checksum = 0; // Set to 0 for checksum calculation
    uint16_t calculated_checksum = icmp_checksum(packet, len);
    if (received_checksum != calculated_checksum) {
        // Checksum mismatch
        return;
    }
    icmp_hdr->checksum = received_checksum; // Restore original checksum

    switch (icmp_hdr->type) {
        case ICMP_ECHO_REQUEST:
            // Handle Echo Request (Ping)
            // Prepare Echo Reply
            icmp_hdr->type = ICMP_ECHO_REPLY;
            icmp_hdr->checksum = 0;
            icmp_hdr->checksum = icmp_checksum(packet, len);
            // Send the reply back (send_packet is a placeholder function)
            ip_send(sender, IP_PROTO_ICMP, packet, len, card);
            break;

        case ICMP_ECHO_REPLY:
            pinging = 0; // Received ping reply
            break;

        case ICMP_DEST_UNREACHABLE:
            // Handle Destination Unreachable (notify sender)
            if (pinging) {
                pinging = -1; // Stop pinging if we get unreachable
            }
            break;

        case ICMP_TIME_EXCEEDED:
            // Handle Time Exceeded (notify sender)
            break;

        case ICMP_TIMESTAMP_REQUEST:
            // Handle Timestamp Request 
            break;

        case ICMP_TIMESTAMP_REPLY:
            // Handle Timestamp Reply
            break;

        case ICMP_REDIRECT:
            // Handle Redirect
            break;

        default:
            // Unknown or unsupported ICMP type
            break;
    }
}

void icmp_send(uint8_t *dest_ip, uint8_t type, uint8_t code, uint16_t identifier, uint16_t sequence_number, uint8_t *data, int data_len, int card) {
    int packet_len = sizeof(icmp_header_t) + data_len;
    uint8_t *packet = (uint8_t *)kmalloc(packet_len);
    if (!packet) {
        return; // Memory allocation failed
    }

    icmp_header_t *icmp_hdr = (icmp_header_t *)packet;
    icmp_hdr->type = type;
    icmp_hdr->code = code;
    icmp_hdr->identifier = htons(identifier);
    icmp_hdr->sequence_number = htons(sequence_number);
    memcpy(packet + sizeof(icmp_header_t), data, data_len);

    icmp_hdr->checksum = 0;
    icmp_hdr->checksum = icmp_checksum(packet, packet_len);

    ip_send(dest_ip, IP_PROTO_ICMP, packet, packet_len, card);

    kfree(packet);
}

int ping(uint8_t *dest_ip, int card) {
    const char ping_data[] = "PING";
    pinging = 1;
    icmp_send(dest_ip, ICMP_ECHO_REQUEST, ICMP_ECHO_REQUEST, 0, 1, (uint8_t*)ping_data, sizeof(ping_data), card);
    uint64_t start = get_uptime_milliseconds();
    while (pinging == 1);
    if (pinging == 0) {
        uint16_t elapsed = get_uptime_milliseconds() - start;
        return elapsed; // Ping successful
    } else {
        return -1; // Ping failed or unreachable
    }
}

void send_unreachable(uint8_t *dest_ip, uint8_t code, uint8_t *original_packet, int original_len, int card) {
    // Send Destination Unreachable message
    icmp_send(dest_ip, ICMP_DEST_UNREACHABLE, code, 0, 0, original_packet, original_len, card);
}


