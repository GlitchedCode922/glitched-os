#pragma once
#include <stdint.h>
#include <uapi/socket.h> // IWYU pragma: export

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
void add_route(uint8_t* dest_ip, uint8_t* gateway, uint8_t* netmask, char* interface);
void remove_route(uint8_t* dest_ip, uint8_t* netmask);
int socket(int domain, int type, int protocol);
int bind(int fd, sockaddr_in_t* addr);
int unbind(int fd);
int64_t recvfrom(int fd, void* buffer, uint64_t len, int flags, sockaddr_in_t* addr);
int64_t sendto(int fd, const void* buffer, uint64_t len, int flags, const sockaddr_in_t* addr);
