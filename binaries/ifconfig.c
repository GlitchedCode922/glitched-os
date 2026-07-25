#include "ioctl.h"
#include <net.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
        printf("Usage: %s interface ip subnet router\n", argv[0]);
        return 1;
    }
    int fd = open(argv[1], 0);
    if (fd < 0) {
        perror(argv[1]);
        return 1;
    }

    if_info_t if_info;
    uint8_t router[4];
    int res = parse_ip(argv[2], if_info.ip);
    res += parse_ip(argv[3], if_info.subnet);
    res += parse_ip(argv[4], router);
    if (res < 0) {
        printf("Invalid IPv4 address");
        return 1;
    }
    
    res = ioctl(fd, IF_CONFIGURE, &if_info);
    if (res < 0) {
        perror("ifconfig");
        return 1;
    }
    add_route(if_info.ip, (uint8_t[4]){0, 0, 0, 0}, if_info.subnet, argv[1]);
    add_route((uint8_t[4]){0, 0, 0, 0}, router, (uint8_t[4]){0, 0, 0, 0}, argv[1]);
    return 0;
}
