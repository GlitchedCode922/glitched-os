#include "socket.h"
#include "../error.h"
#include "../memory/mman.h"
#include "udp.h"
#include <stdint.h>

int socket(int domain, int type, int protocol, socket_t** socket) {
    if (domain != AF_INET) return -EINVAL;
    if (type != SOCK_DGRAM) return -EINVAL;
    if (protocol != IPPROTO_UDP) return -EINVAL;
    *socket = kmalloc(sizeof(socket_t));
    **socket = (socket_t){
        .domain = domain,
        .type = type,
        .protocol = protocol,
        .bind_addr = {0},
    };
    return 0;
}

int bind(socket_t* socket, sockaddr_in_t* addr) {
    if (socket->domain != AF_INET) return -EINVAL;
    if (socket->type != SOCK_DGRAM) return -EINVAL;
    if (socket->protocol != IPPROTO_UDP) return -EINVAL;
    if (!addr) return -EINVAL;
    if (socket->bind_addr.type) {
        unbind(socket);
    }
    socket->bind_addr = *addr;
    udp_open(addr->port);
    return 0;
}

int unbind(socket_t* socket) {
    if (socket->domain != AF_INET) return -EINVAL;
    if (socket->type != SOCK_DGRAM) return -EINVAL;
    if (socket->protocol != IPPROTO_UDP) return -EINVAL;
    if (socket->bind_addr.type) udp_close(socket->bind_addr.port);
    return 0;
}

int64_t recvfrom(socket_t* socket, uint8_t* buffer, uint64_t len, int flags, sockaddr_in_t* addr) {
    if (socket->domain != AF_INET) return -EINVAL;
    if (socket->type != SOCK_DGRAM) return -EINVAL;
    if (socket->protocol != IPPROTO_UDP) return -EINVAL;
    if (!socket->bind_addr.type) {
        for (int i = 40000; i < 65535; i++) {
            if (udp_open(i) < 0) continue;
            socket->bind_addr = (sockaddr_in_t){
                .type = AF_INET,
                .port = i,
            };
            break;
        }
    }
    return udp_get_packet(socket->bind_addr.port, buffer, len, addr, flags | MSG_PEEK);
}

int64_t sendto(socket_t* socket, const uint8_t* buffer, uint64_t len, int flags, const sockaddr_in_t* addr) {
    if (socket->domain != AF_INET) return -EINVAL;
    if (socket->type != SOCK_DGRAM) return -EINVAL;
    if (socket->protocol != IPPROTO_UDP) return -EINVAL;
    if (!socket->bind_addr.type) {
        for (int i = 40000; i < 65535; i++) {
            if (udp_open(i) < 0) continue;
            socket->bind_addr = (sockaddr_in_t){
                .type = AF_INET,
                .port = i,
            };
            break;
        }
    }
    return udp_send((uint8_t*)addr->ip, socket->bind_addr.port, addr->port, (uint8_t*)buffer, len);
}

int socket_close(socket_t *socket) {
    if (socket->domain != AF_INET) return -EINVAL;
    if (socket->type != SOCK_DGRAM) return -EINVAL;
    if (socket->protocol != IPPROTO_UDP) return -EINVAL;
    if (socket->bind_addr.type) udp_close(socket->bind_addr.port);
    kfree(socket);
    return 0;
}
