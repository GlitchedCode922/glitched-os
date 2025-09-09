#pragma once
#include <stdint.h>

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence_number;
} __attribute__((packed)) icmp_header_t;

// ICMP Types
#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8
#define ICMP_DEST_UNREACHABLE 3
#define ICMP_TIME_EXCEEDED 11
#define ICMP_TIMESTAMP_REQUEST 13
#define ICMP_TIMESTAMP_REPLY 14
#define ICMP_REDIRECT 5

uint16_t icmp_checksum(uint8_t* data, int len);
void icmp_received(uint8_t* packet, uint8_t* sender, int len);
void icmp_send(uint8_t* dest_ip, uint8_t type, uint8_t code, uint16_t identifier, uint16_t sequence_number, uint8_t *data, int data_len);
int ping(uint8_t* dest_ip);
void send_unreachable(uint8_t* dest_ip, uint8_t code, uint8_t* original_ip_packet, int original_len);
