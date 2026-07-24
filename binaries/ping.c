#include <net.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

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
    if (argc < 2 || argc > 5) {
        printf("Usage: %s <ip> [count] [timeout] [interval]\n", argv[0]);
        return 1;
    }
    int count = 16;
    if (argc >= 3) count = atoi(argv[2]);
    int timeout = 2500;
    if (argc >= 4) timeout = atoi(argv[3]);
    int interval = 1000;
    if (argc >= 5) interval = atoi(argv[4]);
    uint8_t ip[4];
    int res = parse_ip(argv[1], ip);
    if (res < 0) {
        printf("Invalid IP address\n");
        return 1;
    }
    for (int i = 0; i < count; i++) {
        int ms = ping(ip, timeout);
        if (ms < 0 && errno != ETIMEDOUT) {
            perror("ping");
            return 1;
        } else if (ms < 0) {
            printf("%d: timed out\n", i);
            continue;
        }
        printf("%d: %d ms\n", i, ms);
        if (interval > ms) sleep(interval - ms);
    }
    return 0;
}
