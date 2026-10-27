#pragma once
#include <stdint.h>

#define SPAWN_DUP2 0
#define SPAWN_SIGMASK 1
#define SPAWN_SETPGID 2
#define SPAWN_SETSID 3

typedef struct spawn_action {
    uint64_t action;
    int oldfd;
    int newfd;
    int sigmask_how;
    uint32_t sigmask;
    int pgid;
} __attribute__((packed)) spawn_action_t;
