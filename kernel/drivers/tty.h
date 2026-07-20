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

#define MAX_TTYS 256

typedef struct termios {
    uint64_t c_iflag;
    uint64_t c_oflag;
    uint64_t c_lflag;
} termios_t;

typedef struct tty {
    termios_t termios;
    void* data;
    int64_t (*echo)(void* data, const uint8_t* buffer, uint64_t len);
    int64_t (*write)(void* data, const uint8_t* buffer, uint64_t len);
    char read_buffer[4096];
    int read_head;
    int write_head;
    char line_buffer[1024];
    int line_index;
    char* name;
} tty_t;

void tty_char_recv(int tty_id, char c);
int64_t tty_read(int tty_id, uint64_t offset, uint8_t* buffer, uint64_t len);
int64_t tty_write(int tty_id, uint64_t offset, const uint8_t* buffer, uint64_t len);
int tty_ioctl(int tty_id, uint64_t request, uint64_t arg);
void tty_init();
int register_tty(tty_t* tty);
