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

typedef struct {
    uint8_t domain;
    uint8_t type;
    uint8_t protocol;
    sockaddr_in_t bind_addr;
} socket_t;

int socket(int domain, int type, int protocol, socket_t** socket);
int bind(socket_t* socket, sockaddr_in_t* addr);
int unbind(socket_t* socket);
int64_t recvfrom(socket_t* socket, uint8_t* buffer, uint64_t len, int flags, sockaddr_in_t* addr);
int64_t sendto(socket_t* socket, const uint8_t* buffer, uint64_t len, int flags, const sockaddr_in_t* addr);
int socket_close(socket_t* socket);
