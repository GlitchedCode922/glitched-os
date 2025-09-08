#pragma once
#include <stdint.h>

typedef struct {
    char name[32];
    uint8_t mac[6];
    uint32_t driver;
    uint32_t driver_local_index;
    uint32_t ip;
    uint32_t subnet;
    uint32_t router;
} net_if_t;

typedef struct {
    char name[32];
    void (*send_packet)(int card, void* buffer, int length);
    int (*read_packet)(int card, void** buffer);
    uint8_t* (*get_mac_address)(int card);
} net_driver_t;

int get_if_index_by_name(const char* name);
net_if_t net_get_interface(int index);
int get_global_if_index(int driver, int driver_local_index);
int net_get_interface_count();
int configure_network_interface_dhcp(int index);
int configure_network_interface_static(int index, uint32_t ip, uint32_t subnet, uint32_t router);
int send_packet(int if_index, void* data, int length);
int receive_packet(int if_index, void** buffer);
int does_exist(int if_index);
int get_ip(int if_index, uint32_t* ip);
int get_subnet(int if_index, uint32_t* subnet);
int get_router(int if_index, uint32_t* router);
int get_mac(int if_index, uint8_t* mac);
int rename_interface(int if_index, const char* new_name);

int register_net_driver(net_driver_t driver);
int register_net_interface(int driver, int driver_local_index);
