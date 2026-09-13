#pragma once
#include <stdint.h>
#include "../uapi/termios.h" // IWYU pragma: export

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
