#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "../memory/mman.h"
#include "icmp.h"
#include "udp.h"
#include "../drivers/net/rtl8139.h"
#include <stdint.h>

uint8_t router_ip[4] = {10, 0, 2, 2};
uint8_t subnet_mask[4] = {255, 255, 255, 0};

extern char ip[4];

uint8_t fragment_storage[12][0xFFFF]; // Storage for fragment reassembly
int ids[12]; // Identification numbers for fragments

uint8_t* icmp_error_packet = NULL;

uint16_t identification = 1; // Global identification counter

void ip_send(uint8_t* dst_ip, uint8_t protocol, uint8_t* payload, int payload_length, int card) {
    ipv4_header_t ip_header;
    uint8_t* packet;
    uint16_t total_length;
    uint8_t dst_mac[6] = BROADCAST_MAC; // Default to broadcast
    
    // Determine if the destination IP is in the same subnet
    int same_subnet = 1;
    for (int i = 0; i < 4; i++) {
        if ((ip[i] & subnet_mask[i]) != (dst_ip[i] & subnet_mask[i])) {
            same_subnet = 0;
            break;
        }
    }

    if (memcmp(dst_ip, IP_BROADCAST_ADDR, 4) == 0) {
        // If destination is broadcast address, use broadcast MAC
        memcpy(dst_mac, BROADCAST_MAC, 6);
    } else if (!same_subnet) {
        // If not in the same subnet, send to the router's MAC address
        arp_request(router_ip, dst_mac, card);
    } else {
        // If in the same subnet, resolve the destination MAC address
        arp_request(dst_ip, dst_mac, card);
    }

    // Fill IP header fields
    ip_header.version_ihl = (4 << 4) | (IPV4_HEADER_LEN / 4);
    ip_header.tos = 0;
    total_length = IPV4_HEADER_LEN + payload_length;
    ip_header.total_length = htons(total_length);
    ip_header.identification = htons(identification++);
    ip_header.flags_fragment_offset = htons(IP_FLAG_DF); // Don't Fragment
    ip_header.ttl = IP_DEFAULT_TTL;
    ip_header.protocol = protocol;
    ip_header.header_checksum = 0; // Initial checksum value
    memcpy(ip_header.src_ip, ip, 4);
    memcpy(ip_header.dst_ip, dst_ip, 4);

    // Calculate IP header checksum
    uint16_t* header_words = (uint16_t*)&ip_header;
    uint32_t checksum = 0;
    for (int i = 0; i < IPV4_HEADER_LEN / 2; i++) {
        checksum += ntohs(header_words[i]);
    }
    while (checksum >> 16) {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }
    ip_header.header_checksum = htons(~checksum);

    if (payload_length > 1500) { // More than MTU
        // Fragment the packet
        for (uint16_t i = 0; i < payload_length; i += 1480) {
            // Calculate fragment size
            int fragment_size = (payload_length - i > 1480) ? 1480 : (payload_length - i);
            int fragment_total_length = IPV4_HEADER_LEN + fragment_size;
            uint8_t* fragment_packet = (uint8_t*)kmalloc(fragment_total_length);
            if (!fragment_packet) {
                return; // Memory allocation failed
            }
            // Update IP header for the fragment
            ip_header.total_length = htons(fragment_total_length);
            uint16_t offset = i / 8;
            ip_header.flags_fragment_offset = htons(offset | ((i + fragment_size < payload_length) ? IP_FLAG_MF : 0));
            // Recalculate checksum
            ip_header.header_checksum = 0;
            checksum = 0;
            for (int j = 0; j < IPV4_HEADER_LEN / 2; j++) {
                checksum += ntohs(header_words[j]);
            }
            while (checksum >> 16) {
                checksum = (checksum & 0xFFFF) + (checksum >> 16);
            }
            ip_header.header_checksum = htons(~checksum);
            // Copy header and fragment payload
            memcpy(fragment_packet, &ip_header, IPV4_HEADER_LEN);
            memcpy(fragment_packet + IPV4_HEADER_LEN, payload + i, fragment_size);
            // Send the fragment
            uint8_t* our_mac = rtl8139_get_mac_address(card);
            send_ethernet(our_mac, dst_mac, 0x0800, fragment_packet, fragment_total_length, card);
        }
        return;
    }

    // Allocate memory for the entire packet (IP header + payload)

    packet = (uint8_t*)kmalloc(total_length);
    if (!packet) {
        return; // Memory allocation failed
    }

    // Copy IP header and payload into the packet
    memcpy(packet, &ip_header, IPV4_HEADER_LEN);
    memcpy(packet + IPV4_HEADER_LEN, payload, payload_length);

    // Send the packet via Ethernet
    uint8_t* our_mac = rtl8139_get_mac_address(card);
    send_ethernet((char*)our_mac, (char*)dst_mac, 0x0800, packet, total_length, card);

    // Free allocated memory
    kfree(packet);
}

void ip_received(uint8_t* frame, int card) {
    ipv4_header_t* ip_header = (ipv4_header_t*)frame;

    // Verify IP version and header length
    if ((ip_header->version_ihl >> 4) != 4 || (ip_header->version_ihl & 0x0F) < 5) {
        return; // Not IPv4 or invalid header length
    }

    // Verify destination IP address
    if (memcmp(ip_header->dst_ip, ip, 4) != 0 && memcmp(ip_header->dst_ip, IP_BROADCAST_ADDR, 4) != 0) {
        return; // Not for us
    }

    // Calculate and verify IP header checksum
    uint16_t received_checksum = ip_header->header_checksum;
    ip_header->header_checksum = 0; // Set to 0 for checksum calculation
    uint16_t* header_words = (uint16_t*)ip_header;
    uint32_t checksum = 0;
    for (int i = 0; i < (ip_header->version_ihl & 0x0F) * 2; i++) {
        checksum += ntohs(header_words[i]);
    }
    while (checksum >> 16) {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }
    uint16_t calculated_checksum = htons(~checksum);
    if (received_checksum != calculated_checksum) {
        return; // Checksum mismatch
    }
    // Restore checksum
    ip_header->header_checksum = received_checksum;


    // Extract payload
    int header_length = (ip_header->version_ihl & 0x0F) * 4;
    int payload_length = ntohs(ip_header->total_length) - header_length;
    uint8_t* payload = frame + header_length;

    // Check for fragmentation
    uint16_t flags_offset = ntohs(ip_header->flags_fragment_offset);
    if (!(flags_offset & IP_FLAG_DF)) {
        for (int i = 0; i < 12; i++) {
            if (ids[i] == ntohs(ip_header->identification) || ids[i] == 0) {
                ids[i] = ntohs(ip_header->identification);
                memcpy(fragment_storage[i] + (flags_offset & IP_FRAGMENT_OFFSET_MASK) * 8, payload, payload_length);
                
                // If packet does not have MF flag, reassembly completed
                if (!(flags_offset & IP_FLAG_MF)) {
                    // Free the ID slot
                    ids[i] = 0;
                    payload = fragment_storage[i];
                    break;
                } else {
                    return; // Wait for more fragments
                }
            }
        }
    }

    icmp_error_packet = frame; // For ICMP error messages

    // Handle based on protocol
    switch (ip_header->protocol) {
        case IP_PROTO_ICMP:
            icmp_received(payload, ip_header->src_ip, payload_length, card);
            break;
        case IP_PROTO_TCP:
            // Handle TCP (not implemented here)
            send_unreachable(ip_header->src_ip, 2, frame, ntohs(ip_header->total_length) > 20 + 64 ? 20 + 64 : ntohs(ip_header->total_length), card);
            break;
        case IP_PROTO_UDP:
            udp_received(payload, ip_header->src_ip, ip_header->dst_ip, payload_length, card);
            break;
        default:
            // Unsupported protocol
            send_unreachable(ip_header->src_ip, 2, frame, ntohs(ip_header->total_length) > 20 + 64 ? 20 + 64 : ntohs(ip_header->total_length), card);
            break;
    }
}

void ip_send_dest_unreachable(uint8_t *dest_ip, uint8_t code, int card) {
    if (icmp_error_packet) {
        // Get packet length
        int len = ntohs(((ipv4_header_t*)icmp_error_packet)->total_length);
        send_unreachable(dest_ip, code, icmp_error_packet, len, card);
    }
}
