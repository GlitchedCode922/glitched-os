#include "net.h"
#include "../error.h"
#include "chrdev.h"
#include <stddef.h>
#include <stdint.h>
#include "../memory/mman.h"
#include "../ioctl_list.h"
#include "../vfs.h"
#include "../fs/devfs.h"

net_if_t net_interfaces[10];
int net_interface_count = 0;
net_driver_t net_drivers[10];
int net_driver_count = 0;
int net_driver_index = 0;

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

int get_global_if_index(int driver, int driver_local_index) {
    for (int i = 0; i < net_interface_count; i++) {
        if (net_interfaces[i].driver == driver && net_interfaces[i].driver_local_index == driver_local_index) {
            return i;
        }
    }
    return -ENODEV; // Not found
}

int net_get_interface_count() {
    return net_interface_count;
}

int configure_network_interface_static(int index, uint8_t* ip, uint8_t* subnet) {
    if (index < 0 || index >= net_interface_count) {
        return -ENODEV; // Invalid index
    }
    memcpy(net_interfaces[index].ip, ip, 4);
    memcpy(net_interfaces[index].subnet, subnet, 4);
    return 0; // Success
}

int send_packet(int if_index, void* data, int length) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -ENODEV; // Invalid interface index
    }
    net_if_t* iface = &net_interfaces[if_index];
    if (iface->driver < 0 || iface->driver >= net_driver_count) {
        return -ENODEV; // Invalid driver index
    }
    net_driver_t* driver = &net_drivers[iface->driver];
    if (driver->send_packet == NULL) {
        return -ENOSYS; // Driver does not support sending packets
    }
    driver->send_packet(iface->driver_local_index, data, length);
    return 0; // Success
}

int receive_packet(int if_index, void **buffer) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -ENODEV; // Invalid interface index
    }
    net_if_t* iface = &net_interfaces[if_index];
    if (iface->driver < 0 || iface->driver >= net_driver_count) {
        return -ENODEV; // Invalid driver index
    }
    net_driver_t* driver = &net_drivers[iface->driver];
    if (driver->read_packet == NULL) {
        return -ENOSYS; // Driver does not support reading packets
    }
    return driver->read_packet(iface->driver_local_index, buffer);
}

int does_exist(int if_index) {
    return if_index >= 0 && if_index < net_interface_count;
}

int get_ip(int if_index, uint8_t* ip) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -ENODEV; // Invalid interface index
    }
    memcpy(ip, net_interfaces[if_index].ip, 4);
    return 0; // Success
}

int get_subnet(int if_index, uint8_t* subnet) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -ENODEV; // Invalid interface index
    }
    memcpy(subnet, net_interfaces[if_index].subnet, 4);
    return 0; // Success
}

int get_mac(int if_index, uint8_t* mac) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -ENODEV; // Invalid interface index
    }
    for (int i = 0; i < 6; i++) {
        mac[i] = net_interfaces[if_index].mac[i];
    }
    return 0; // Success
}

int register_net_driver(net_driver_t driver) {
    if (net_driver_count >= 10) {
        return -ENOSPC; // No space for more drivers
    }
    net_drivers[net_driver_count] = driver;
    net_driver_count++;
    return net_driver_count - 1; // Return the index of the newly added driver
}

int register_net_interface(int driver, int driver_local_index) {
    if (net_interface_count >= 10) {
        return -ENOSPC; // No space for more interfaces
    }
    if (driver < 0 || driver >= net_driver_count) {
        return -ENODEV; // Invalid driver index
    }
    net_if_t* iface = &net_interfaces[net_interface_count];
    iface->driver = driver;
    iface->driver_local_index = driver_local_index;
    memset(iface->ip, 0, 4);
    memset(iface->subnet, 0, 4);
    // Generate a default name like "eth0", "eth1", etc.
    char default_name[32] = "eth";
    // Append the interface count to the name
    int len = 3;
    if (net_interface_count < 10) {
        default_name[len++] = '0' + net_interface_count;
    } else if (net_interface_count < 100) {
        default_name[len++] = '0' + (net_interface_count / 10);
        default_name[len++] = '0' + (net_interface_count % 10);
    }
    default_name[len] = '\0';
    devfs_mknod(default_name, DT_CHAR, makedev(net_driver_index, net_interface_count));

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

int net_ioctl(int if_index, uint64_t request, uint64_t arg) {
    if (if_index < 0 || if_index >= net_interface_count) {
        return -ENODEV; // Invalid interface index
    }
    if_info_t* ptr = (if_info_t*)arg;
    if (request == IF_GET_INFO) {
        memcpy(ptr->mac, net_interfaces[if_index].mac, 6);
        memcpy(ptr->ip, net_interfaces[if_index].ip, 4);
        memcpy(ptr->subnet, net_interfaces[if_index].subnet, 4);
        return 0;
    } else if (request == IF_CONFIGURE) {
        memcpy(net_interfaces[if_index].ip, ptr->ip, 4);
        memcpy(net_interfaces[if_index].subnet, ptr->subnet, 4);
        return 0;
    }
    return -ENOTTY;
}

void net_init() {
    char_driver_t net_driver = {
        .ioctl = net_ioctl
    };
    net_driver_index = register_char_driver(&net_driver);
}
