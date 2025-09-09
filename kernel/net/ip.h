#pragma once
#include <stdint.h>

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17
#define IPV4_HEADER_LEN 20
#define IP_MAX_PACKET_SIZE 65535
#define IP_BROADCAST_ADDR "\xff\xff\xff\xff"
#define IP_ADDR_ANY "\x00\x00\x00\x00"
#define IP_DEFAULT_TTL 128
#define IP_FRAGMENT_OFFSET_MASK 0x1FFF
#define IP_FLAG_DF 0x4000 // Don't Fragment
#define IP_FLAG_MF 0x2000 // More Fragments
#define IP_MAX_REASSEMBLY_BUFFERS 16
#define IP_REASSEMBLY_TIMEOUT 30 // seconds
#define IP_REASSEMBLY_BUFFER_SIZE 65535
#define IP_HEADER_CHECKSUM_OFFSET 10
#define IP_HEADER_CHECKSUM_LEN 10

typedef struct {
    uint8_t version_ihl; // Version (4 bits) + Internet header length (4 bits)
    uint8_t tos;         // Type of service
    uint16_t total_length; // Total length
    uint16_t identification; // Identification
    uint16_t flags_fragment_offset; // Flags (3 bits) + Fragment offset (13 bits)
    uint8_t ttl;         // Time to live
    uint8_t protocol;    // Protocol
    uint16_t header_checksum; // Header checksum
    uint8_t src_ip[4];  // Source address
    uint8_t dst_ip[4];  // Destination address
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint8_t* buffer;
    uint16_t total_length;
    uint16_t received_length;
    uint16_t identification;
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint8_t protocol;
    uint32_t last_update; // Timestamp of the last fragment received
} __attribute__((packed)) ip_reassembly_buffer_t;

typedef struct {
    uint8_t dest_ips[4];
    uint8_t gateway[4];
    uint8_t netmask[4];
    int card;
} route_t;

void ip_send(uint8_t* dst_ip, uint8_t protocol, uint8_t* payload, int payload_length);
void ip_received(uint8_t* frame, int card);

void ip_send_dest_unreachable(uint8_t* dest_ip, uint8_t code);
uint32_t get_source_ip_for(uint8_t* dest_ip);
void add_route(uint8_t* dest_ip, uint8_t* gateway, uint8_t* netmask, int card);
void remove_route(uint8_t* dest_ip, uint8_t* netmask);
void setup_automatic_routing();
