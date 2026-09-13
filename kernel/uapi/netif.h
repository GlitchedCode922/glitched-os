#pragma once
#include <stdint.h>

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t subnet[4];
} __attribute__((packed)) if_info_t;
