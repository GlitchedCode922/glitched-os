#include "net.h"
#include "../net/dhcp.h"
#include <stddef.h>

net_if_t net_interfaces[10];
int net_interface_count = 0;
net_driver_t net_drivers[10];
int net_driver_count = 0;

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0'; // Null-terminate the destination string
    return i;
}

int get_if_index_by_name(const char* name) {
    for (int i = 0; i < net_interface_count; i++) {
        if (strcmp(net_interfaces[i].name, name) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

net_if_t net_get_interface(int index) {
    if (index < 0 || index >= net_interface_count) {
        net_if_t empty = {0};
        return empty; // Return an empty struct if index is out of bounds
    }
    return net_interfaces[index];
}

int get_global_if_index(int driver, int driver_local_index) {
    for (int i = 0; i < net_interface_count; i++) {
        if (net_interfaces[i].driver == driver && net_interfaces[i].driver_local_index == driver_local_index) {
            return i;
        }
    }
    return -1; // Not found
}

int net_get_interface_count() {
    return net_interface_count;
}

int configure_network_interface_dhcp(int index) {
    if (index < 0 || index >= net_interface_count) {
        return -1; // Invalid index
    }
    dhcp_run(index);
    return 0; // Success
}

int configure_network_interface_static(int index, uint32_t ip, uint32_t subnet, uint32_t router) {
    if (index < 0 || index >= net_interface_count) {
        return -1; // Invalid index
    }
    net_interfaces[index].ip = ip;
    net_interfaces[index].subnet = subnet;
    net_interfaces[index].router = router;
    return 0; // Success
}

int send_packet(int if_index, void* data, int length) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    net_if_t* iface = &net_interfaces[if_index];
    if (iface->driver < 0 || iface->driver >= net_driver_count) {
        return -2; // Invalid driver index
    }
    net_driver_t* driver = &net_drivers[iface->driver];
    if (driver->send_packet == NULL) {
        return -3; // Driver does not support sending packets
    }
    driver->send_packet(iface->driver_local_index, data, length);
    return 0; // Success
}

int receive_packet(int if_index, void **buffer) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    net_if_t* iface = &net_interfaces[if_index];
    if (iface->driver < 0 || iface->driver >= net_driver_count) {
        return -2; // Invalid driver index
    }
    net_driver_t* driver = &net_drivers[iface->driver];
    if (driver->read_packet == NULL) {
        return -3; // Driver does not support reading packets
    }
    return driver->read_packet(iface->driver_local_index, buffer);
}

int does_exist(int if_index) {
    return if_index >= 0 && if_index < net_interface_count;
}

int get_ip(int if_index, uint32_t* ip) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    *ip = net_interfaces[if_index].ip;
    return 0; // Success
}

int get_subnet(int if_index, uint32_t* subnet) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    *subnet = net_interfaces[if_index].subnet;
    return 0; // Success
}

int get_router(int if_index, uint32_t* router) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    *router = net_interfaces[if_index].router;
    return 0; // Success
}

int get_mac(int if_index, uint8_t* mac) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    for (int i = 0; i < 6; i++) {
        mac[i] = net_interfaces[if_index].mac[i];
    }
    return 0; // Success
}

int rename_interface(int if_index, const char* new_name) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -1; // Invalid interface index
    }
    strncpy(net_interfaces[if_index].name, new_name, sizeof(net_interfaces[if_index].name) - 1);
    net_interfaces[if_index].name[sizeof(net_interfaces[if_index].name) - 1] = '\0'; // Ensure null-termination
    return 0; // Success
}

int register_net_driver(net_driver_t driver) {
    if (net_driver_count >= 10) {
        return -1; // No space for more drivers
    }
    net_drivers[net_driver_count] = driver;
    net_driver_count++;
    return net_driver_count - 1; // Return the index of the newly added driver
}

int register_net_interface(int driver, int driver_local_index) {
    if (net_interface_count >= 10) {
        return -1; // No space for more interfaces
    }
    if (driver < 0 || driver >= net_driver_count) {
        return -2; // Invalid driver index
    }
    net_if_t* iface = &net_interfaces[net_interface_count];
    iface->driver = driver;
    iface->driver_local_index = driver_local_index;
    iface->ip = 0;
    iface->subnet = 0;
    iface->router = 0;
    // Generate a default name like "eth0", "eth1", etc.
    char default_name[32] = "eth";
    // Append the interface count to the name
    int len;
    if (net_interface_count < 10) {
        default_name[len++] = '0' + net_interface_count;
        len = 4;
    } else if (net_interface_count < 100) {
        default_name[len++] = '0' + (net_interface_count / 10);
        default_name[len++] = '0' + (net_interface_count % 10);
        len = 5;
    }
    default_name[len] = '\0';
    strncpy(iface->name, default_name, sizeof(iface->name) - 1);
    iface->name[sizeof(iface->name) - 1] = '\0'; // Ensure null-termination
    
    // Initialize MAC address to zero
    for (int i = 0; i < 6; i++) {
        iface->mac[i] = 0;
    }

    // Get the MAC address from the driver
    net_driver_t* drv = &net_drivers[driver];
    if (drv->get_mac_address != NULL) {
        uint8_t* mac = drv->get_mac_address(driver_local_index);
        if (mac != NULL) {
            for (int i = 0; i < 6; i++) {
                iface->mac[i] = mac[i];
            }
        } else {
            for (int i = 0; i < 6; i++) {
                iface->mac[i] = 0;
            }
        }
    } else {
        for (int i = 0; i < 6; i++) {
            iface->mac[i] = 0;
        }
    }
    net_interface_count++;
    return net_interface_count - 1; // Return the index of the newly added interface
}
