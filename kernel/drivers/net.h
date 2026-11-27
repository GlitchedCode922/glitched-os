#pragma once
#include <stdint.h>
#include "uapi/netif.h" // IWYU pragma: export

extern int net_driver_index;

typedef struct {
    uint8_t mac[6];
    uint32_t driver;
    uint32_t driver_local_index;
    uint8_t ip[4];
    uint8_t subnet[4];
} net_if_t;

typedef struct {
    int (*send_packet)(int card, void* buffer, int length);
    int (*get_mac_address)(int card, uint8_t mac[6]);
} net_driver_t;

int get_global_if_index(int driver, int driver_local_index);
int send_packet(int if_index, void* data, int length);
int get_ip(int if_index, uint8_t* ip);
int get_subnet(int if_index, uint8_t* subnet);
int get_mac(int if_index, uint8_t* mac);

int register_net_driver(net_driver_t driver);
int register_net_interface(int driver, int driver_local_index);
void net_init();

