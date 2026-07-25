#pragma once
#include <stdint.h>

extern int net_driver_index;

typedef struct {
    uint8_t mac[6];
    uint32_t driver;
    uint32_t driver_local_index;
    uint8_t ip[4];
    uint8_t subnet[4];
} net_if_t;

typedef struct {
    void (*send_packet)(int card, void* buffer, int length);
    int (*read_packet)(int card, void** buffer);
    uint8_t* (*get_mac_address)(int card);
} net_driver_t;

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t subnet[4];
} if_info_t;

int get_global_if_index(int driver, int driver_local_index);
int send_packet(int if_index, void* data, int length);
int receive_packet(int if_index, void** buffer);
int get_ip(int if_index, uint8_t* ip);
int get_subnet(int if_index, uint8_t* subnet);
int get_mac(int if_index, uint8_t* mac);

int register_net_driver(net_driver_t driver);
int register_net_interface(int driver, int driver_local_index);
void net_init();

