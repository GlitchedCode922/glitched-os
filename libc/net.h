#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t type;
    uint8_t ip[4];
    uint16_t port;
} __attribute__((packed)) sockaddr_in_t;

int ping(uint8_t* dest_ip);
int configure_network_interface_static(int index, uint32_t ip, uint32_t subnet, uint32_t router);
void add_route(uint8_t* dest_ip, uint8_t* gateway, uint8_t* netmask, int card);
void remove_route(uint8_t* dest_ip, uint8_t* netmask);
void setup_automatic_routing();
int socket(int domain, int type, int protocol);
int bind(int fd, sockaddr_in_t* addr);
int unbind(int fd);
int64_t recvfrom(int fd, uint8_t* buffer, uint64_t len, int flags, sockaddr_in_t* addr);
int64_t sendto(int fd, const uint8_t* buffer, uint64_t len, int flags, const sockaddr_in_t* addr);
