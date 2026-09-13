#pragma once
#include <stdint.h>

#define AF_INET 1
#define SOCK_DGRAM 1
#define IPPROTO_UDP 1

#define MSG_PEEK 1

typedef struct {
    uint8_t type;
    uint8_t ip[4];
    uint16_t port;
} __attribute__((packed)) sockaddr_in_t;
