#include "ioctl.h"
#include <stdint.h>
#include <net.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_MAGIC_COOKIE 0x63825363
#define DHCP_OPTION_END 255
#define DHCP_OPTION_MESSAGE_TYPE 53
#define DHCP_OPTION_REQUESTED_IP 50
#define DHCP_OPTION_SERVER_IDENTIFIER 54
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS 6
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5
#define DHCP_NAK 6

typedef struct {
    uint8_t op; // Message op code / message type
    uint8_t htype; // Hardware address type
    uint8_t hlen; // Hardware address length
    uint8_t hops; // Hops
    uint32_t xid; // Transaction ID
    uint16_t secs; // Seconds elapsed
    uint16_t flags; // Flags
    uint8_t ciaddr[4]; // Client IP address
    uint8_t yiaddr[4]; // Your IP address
    uint8_t siaddr[4]; // Next server IP address
    uint8_t giaddr[4]; // Relay agent IP address
    uint8_t chaddr[16]; // Client hardware address
    uint8_t sname[64]; // Optional server host name
    uint8_t file[128]; // Boot file name
    uint32_t magic_cookie; // Magic cookie
    uint8_t options[312]; // Optional parameters field
} __attribute__((packed)) dhcp_packet_t;

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    sockaddr_in_t client_addr = {
        .type = AF_INET,
        .ip = {0, 0, 0, 0},
        .port = DHCP_CLIENT_PORT,
    };

    sockaddr_in_t server_addr = {
        .type = AF_INET,
        .ip = {255, 255, 255, 255},
        .port = DHCP_SERVER_PORT,
    };

    int res = bind(sock, &client_addr);
    if (res < 0) {
        perror("bind");
        return 1;
    }

    uint32_t transaction_id = 0x12345678; // Example transaction ID
    uint8_t client_mac[6];
    uint8_t offered_ip[4];
    uint8_t dhcp_server_ip[4];

    if_info_t if_info;
    // Get MAC address of the network card
    int if_fd = open("/dev/eth0", O_RDONLY);
    res = ioctl(if_fd, IF_GET_INFO, &if_info);
    if (res < 0) {
        perror("ioctl");
        return 1;
    }

    // Construct DHCP DISCOVER packet
    dhcp_packet_t dhcp_discover = {0};
    dhcp_discover.op = 1; // BOOTREQUEST
    dhcp_discover.htype = 1; // Ethernet
    dhcp_discover.hlen = 6; // MAC address length
    dhcp_discover.hops = 0;
    dhcp_discover.xid = htonl(transaction_id);
    dhcp_discover.secs = 0;
    dhcp_discover.flags = htons(0x8000); // Broadcast flag
    memcpy(dhcp_discover.chaddr, client_mac, 6);
    dhcp_discover.magic_cookie = htonl(DHCP_MAGIC_COOKIE);
    uint8_t* options = dhcp_discover.options;
    options[0] = DHCP_OPTION_MESSAGE_TYPE;
    options[1] = 1;
    options[2] = DHCP_DISCOVER;
    options[3] = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    options[4] = 3; // Length
    options[5] = DHCP_OPTION_SUBNET_MASK;
    options[6] = DHCP_OPTION_ROUTER;
    options[7] = DHCP_OPTION_DNS;
    options[8] = DHCP_OPTION_END;

    res = sendto(sock, &dhcp_discover, sizeof(dhcp_packet_t), 0, &server_addr);
    sockaddr_in_t sender = {0};
    dhcp_packet_t dhcp_offer;
    while (sender.port != DHCP_SERVER_PORT) {
        res = recvfrom(sock, &dhcp_offer, sizeof(dhcp_packet_t), 0, &sender);
        if (res < 0) {
            perror("recvfrom");
            return 1;
        }
    }
    if (res < 240) {
        printf("DHCP offer packet truncated\n");
        return 1;
    }
    if (dhcp_offer.magic_cookie != htonl(DHCP_MAGIC_COOKIE)) {
        printf("DHCP offer packet invalid\n");
        return 1;
    }

    // Parse DHCP OFFER packet
    memcpy(offered_ip, dhcp_offer.yiaddr, 4);
    // Extract DHCP server IP from options
    uint8_t* opt_ptr = dhcp_offer.options;
    while (*opt_ptr != DHCP_OPTION_END) {
        if (*opt_ptr == DHCP_OPTION_SERVER_IDENTIFIER && *(opt_ptr + 1) == 4) {
            memcpy(dhcp_server_ip, opt_ptr + 2, 4);
            break;
        }
        opt_ptr += 2 + *(opt_ptr + 1);
    }

    // Construct DHCP REQUEST packet
    dhcp_packet_t dhcp_request = {0};
    dhcp_request.op = 1; // BOOTREQUEST
    dhcp_request.htype = 1; // Ethernet
    dhcp_request.hlen = 6; // MAC address length
    dhcp_request.hops = 0;
    dhcp_request.xid = htonl(transaction_id);
    dhcp_request.secs = 0;
    dhcp_request.flags = htons(0x8000); // Broadcast flag
    memcpy(dhcp_request.chaddr, client_mac, 6);
    dhcp_request.magic_cookie = htonl(DHCP_MAGIC_COOKIE);
    options = dhcp_request.options;
    options[0] = DHCP_OPTION_MESSAGE_TYPE;
    options[1] = 1;
    options[2] = DHCP_REQUEST;
    options[3] = DHCP_OPTION_REQUESTED_IP;
    options[4] = 4;
    memcpy(&options[5], offered_ip, 4);
    options[9] = DHCP_OPTION_SERVER_IDENTIFIER;
    options[10] = 4;
    memcpy(&options[11], dhcp_server_ip, 4);
    options[15] = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    options[16] = 3; // Length
    options[17] = DHCP_OPTION_SUBNET_MASK;
    options[18] = DHCP_OPTION_ROUTER;
    options[19] = DHCP_OPTION_DNS;
    options[20] = DHCP_OPTION_END;

    res = sendto(sock, &dhcp_request, sizeof(dhcp_packet_t), 0, &server_addr);
    dhcp_packet_t dhcp_ack;
    sender = (sockaddr_in_t){0};
    while (sender.port != DHCP_SERVER_PORT) {
        res = recvfrom(sock, &dhcp_ack, sizeof(dhcp_packet_t), 0, &sender);
        if (res < 0) {
            perror("recvfrom");
            return 1;
        }
    }
    if (res < 240) {
        printf("DHCP ACK packet truncated\n");
        return 1;
    }
    if (dhcp_ack.magic_cookie != htonl(DHCP_MAGIC_COOKIE)) {
        printf("DHCP ACK packet invalid\n");
        return 1;
    }

    uint8_t ip[4];
    uint8_t subnet_mask[4];
    uint8_t router_ip[4];
    // Parse DHCP ACK packet
    memcpy(ip, dhcp_ack.yiaddr, 4);
    // Extract subnet mask and router from options
    opt_ptr = dhcp_ack.options;
    while (*opt_ptr != DHCP_OPTION_END) {
        if (*opt_ptr == DHCP_OPTION_SUBNET_MASK && *(opt_ptr + 1) == 4) {
            memcpy(subnet_mask, opt_ptr + 2, 4);
        } else if (*opt_ptr == DHCP_OPTION_ROUTER && *(opt_ptr + 1) >= 4) {
            memcpy(router_ip, opt_ptr + 2, 4);
        }
        opt_ptr += 2 + *(opt_ptr + 1);
    }

    memcpy(if_info.ip, ip, 4);
    memcpy(if_info.subnet, subnet_mask, 4);
    ioctl(if_fd, IF_CONFIGURE, &if_info);
    add_route(ip, (uint8_t[4]){0, 0, 0, 0}, subnet_mask, "/dev/eth0");
    add_route((uint8_t[4]){0, 0, 0, 0}, router_ip, (uint8_t[4]){0, 0, 0, 0}, "/dev/eth0");

    return 0;
}
