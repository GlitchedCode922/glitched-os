#pragma once
#include <stdint.h>

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

#define UDP_HEADER_SIZE 8
#define UDP_MAX_DATA_SIZE 1472 // 1500 - 20 (IP header) - 8 (UDP header)
#define UDP_MAX_PACKET_SIZE (UDP_HEADER_SIZE + UDP_MAX_DATA_SIZE)

uint16_t udp_checksum(uint8_t* src_ip, uint8_t* dest_ip, uint8_t* udp_packet, int len);
void udp_received(uint8_t* packet, uint8_t* sender, uint8_t* dest, int len, int card);
void udp_send(uint8_t* dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t* data, int data_len, int card);

void register_udp_listener(uint16_t port, void (*listener)(uint8_t*, uint16_t, uint8_t*, int));
void unregister_udp_listener(uint16_t port);
