#pragma once
#include <stddef.h>
#include <stdint.h>

#define ICRNL 0x1
#define ISTRIP 0x2

#define OPOST 0x1
#define ONLCR 0x2
#define OCRNL 0x4

#define ICANON 0x1
#define ECHO 0x2
#define ECHOE 0x4
#define ECHOCTL 0x8

typedef struct termios {
    uint64_t c_iflag;
    uint64_t c_oflag;
    uint64_t c_lflag;
} termios_t;

typedef struct tty {
    termios_t termios;
    int port;
    size_t (*echo)(struct tty* tty, const char* buffer, size_t len);
    size_t (*write)(struct tty* tty, const char* buffer, size_t len);
    char read_buffer[4096];
    int read_head;
    int write_head;
    char line_buffer[1024];
    int line_index;
} tty_t;

void tty_char_recv(tty_t* tty, char c);
size_t tty_read(tty_t* tty, char* buffer, size_t len, int block);
size_t tty_write(tty_t* tty, const char* buffer, size_t len);
