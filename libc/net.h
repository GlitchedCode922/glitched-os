#pragma once
#include <stdint.h>
#include <stddef.h>

#define AF_INET 1
#define SOCK_DGRAM 1
#define IPPROTO_UDP 1

#define MSG_PEEK 1

typedef struct {
    uint8_t type;
    uint8_t ip[4];
    uint16_t port;
} __attribute__((packed)) sockaddr_in_t;

static inline uint16_t htons(uint16_t val) {
    return (val >> 8) | (val << 8);
}

static inline uint16_t ntohs(uint16_t val) {
    return (val >> 8) | (val << 8);
}

static inline uint32_t htonl(uint32_t val) {
    return ((val >> 24) & 0x000000FF) |
           ((val >> 8)  & 0x0000FF00) |
           ((val << 8)  & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

static inline uint32_t ntohl(uint32_t val) {
    return ((val >> 24) & 0x000000FF) |
           ((val >> 8)  & 0x0000FF00) |
           ((val << 8)  & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

int ping(uint8_t* dest_ip, int timeout);
int configure_network_interface_static(int index, uint32_t ip, uint32_t subnet, uint32_t router);
void add_route(uint8_t* dest_ip, uint8_t* gateway, uint8_t* netmask, int card);
void remove_route(uint8_t* dest_ip, uint8_t* netmask);
void setup_automatic_routing();
int socket(int domain, int type, int protocol);
int bind(int fd, sockaddr_in_t* addr);
int unbind(int fd);
int64_t recvfrom(int fd, void* buffer, uint64_t len, int flags, sockaddr_in_t* addr);
int64_t sendto(int fd, const void* buffer, uint64_t len, int flags, const sockaddr_in_t* addr);
int getmacaddr(int if_index, uint8_t* mac);
int getipaddr(int if_index, uint8_t* ip);
