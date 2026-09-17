#pragma once
#include <stdint.h>
#include "uapi/socket.h" // IWYU pragma: export

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
