#include <net.h>
#include <stdint.h>
#include <stdio.h>

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
    if (argc != 2) {
        printf("Usage: %s ip\n", argv[0]);
        return 1;
    }
    uint8_t ip[4];
    int res = parse_ip(argv[1], ip);
    if (res < 0) {
        printf("Invalid IP address\n");
        return 1;
    }
    for (int i = 0; i < 16; i++) {
        int ms = ping(ip);
        if (ms < 0) {
            perror("ping");
            return 1;
        }
        printf("%d: %d ms\n", i, ms);
    }
    return 0;
}
