#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

#define UDP_HEADER_SIZE 8
#define UDP_MAX_DATA_SIZE 1472 // 1500 - 20 (IP header) - 8 (UDP header)
#define UDP_MAX_PACKET_SIZE (UDP_HEADER_SIZE + UDP_MAX_DATA_SIZE)
#define UDP_BUFFER_SIZE 1024

uint16_t udp_checksum(uint8_t* src_ip, uint8_t* dest_ip, uint8_t* udp_packet, int len);
void udp_received(uint8_t* packet, uint8_t* sender, uint8_t* dest, int len);
int udp_send(uint8_t* dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t* data, int data_len);
int64_t udp_get_packet(uint8_t* source, uint16_t port, uint8_t* payload, size_t len, int peek);
int udp_open(uint16_t port_number);
void udp_close(uint16_t port_number);
