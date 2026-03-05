#pragma once
#include <stdint.h>

typedef struct termios {
    uint64_t c_iflag;
    uint64_t c_oflag;
    uint64_t c_lflag;
} termios_t;

#define ICRNL 0x1
#define ISTRIP 0x2

#define OPOST 0x1
#define ONLCR 0x2
#define OCRNL 0x4

#define ICANON 0x1
#define ECHO 0x2
#define ECHOE 0x3

int isatty(int fd);
int tcgetattr(int fd, struct termios* p_termios);
int tcsetattr(int fd, struct termios* p_termios);
