#pragma once
#include <stdint.h>

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

void dhcp_run(int card);
void dhcp_listener(uint8_t* sender_ip, uint16_t sender_port, uint8_t* data, int len);
