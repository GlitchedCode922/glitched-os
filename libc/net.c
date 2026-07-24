#include "net.h"
#include "syscall.h"
#include "errno.h"
#include <stdint.h>

int ping(uint8_t *dest_ip, int timeout) {
    int64_t res = syscall(SYSCALL_PING, (uint64_t)dest_ip, timeout, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int configure_network_interface_static(int index, uint32_t ip, uint32_t subnet, uint32_t router) {
    int64_t res = syscall(SYSCALL_CONFIG_STATIC, index, ip, subnet, router, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

void add_route(uint8_t* dest_ip, uint8_t* gateway, uint8_t* netmask, int card) {
    syscall(SYSCALL_ADD_ROUTE, (uint64_t)dest_ip, (uint64_t)gateway, (uint64_t)netmask, card, 0, 0);
}

void remove_route(uint8_t* dest_ip, uint8_t* netmask) {
    syscall(SYSCALL_REMOVE_ROUTE, (uint64_t)dest_ip, (uint64_t)netmask, 0, 0, 0, 0);
}

void setup_automatic_routing() {
    syscall(SYSCALL_SETUP_AUTOMATIC_ROUTING, 0, 0, 0, 0, 0, 0);
}

int socket(int domain, int type, int protocol) {
    int64_t res = syscall(SYSCALL_SOCKET, domain, type, protocol, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int bind(int fd, sockaddr_in_t* addr) {
    int64_t res = syscall(SYSCALL_BIND, fd, (uint64_t)addr, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int unbind(int fd) {
    int64_t res = syscall(SYSCALL_UNBIND, fd, 0, 0, 0, 0, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int64_t recvfrom(int fd, uint8_t* buffer, uint64_t len, int flags, sockaddr_in_t* addr) {
    int64_t res = syscall(SYSCALL_RECVFROM, fd, (uint64_t)buffer, len, flags, (uint64_t)addr, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

int64_t sendto(int fd, const uint8_t* buffer, uint64_t len, int flags, const sockaddr_in_t* addr) {
    int64_t res = syscall(SYSCALL_SENDTO, fd, (uint64_t)buffer, len, flags, (uint64_t)addr, 0);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}
