#include <net.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_ip(char* string, uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        uint16_t value = 0;
        if (*string < '0' || *string > '9') return -1;
        while (*string >= '0' && *string <= '9') {
            value = value * 10 + (*string - '0');
            if (value > 255) return -1;
            string++;
        }
        ip[i] = (uint8_t)value;
        if (i < 3) {
            if (*string != '.') return -1;
            string++;
        } else {
            if (*string != '\0') return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        printf("Usage: %s if_id ip subnet router\n", argv[0]);
        return 1;
    }
    int interface = atoi(argv[1]);
    uint8_t ip[4];
    uint8_t subnet[4];
    uint8_t router[4];
    int res = parse_ip(argv[2], ip);
    res += parse_ip(argv[3], subnet);
    res += parse_ip(argv[4], router);
    if (res < 0) {
        printf("Invalid IPv4 address");
        return 1;
    }

    res = configure_network_interface(interface, *(uint32_t*)ip, *(uint32_t*)subnet);
    if (res < 0) {
        perror("ifconfig");
        return 1;
    }
    add_route(ip, (uint8_t[4]){0, 0, 0, 0}, subnet, interface);
    add_route((uint8_t[4]){0, 0, 0, 0}, router, (uint8_t[4]){0, 0, 0, 0}, interface);
    return 0;
}
