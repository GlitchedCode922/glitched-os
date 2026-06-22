#include "tty.h"
#include <stddef.h>
#include <stdint.h>
#include "../error.h"
#include "chrdev.h"
#include "../ioctl_list.h"

tty_t* ttys[MAX_TTYS];
int tty_count = 0;
int tty_driver_index = 0;

static inline int is_printable(char c) {
    return c >= 0x20 && c <= 0x7E;
}

void tty_char_recv(int tty_id, char c) {
    if (tty_id < 0) return;
    if (tty_id >= tty_count) return;
    tty_t* tty = ttys[tty_id];
    int erased = 0;
    if (tty->termios.c_iflag & ISTRIP) c &= 0b01111111;
    if (tty->termios.c_iflag & ICRNL && c == '\r') c = '\n';
    if (tty->termios.c_lflag & ICANON) {
        if (c == '\x7f') {
            if (tty->line_index > 0) {
                tty->line_index--;
                erased = 1;
            }
        } else if (c == '\n') {
            tty->line_buffer[tty->line_index] = c;
            tty->line_index++;
            for (int i = 0; i < tty->line_index; i++) {
                if ((tty->write_head + 1) % 4096 == tty->read_head) return;
                tty->read_buffer[tty->write_head++] = tty->line_buffer[i];
                tty->write_head %= 4096;
            }
            tty->line_index = 0;
        } else {
            if (tty->line_index >= 1022) return;
            tty->line_buffer[tty->line_index] = c;
            tty->line_index++;
        }
    } else {
        if ((tty->write_head + 1) % 4096 == tty->read_head) return;
        tty->read_buffer[tty->write_head++] = c;
        tty->write_head %= 4096;
    }
    if (tty->termios.c_lflag & ECHOE && tty->termios.c_lflag & ICANON && c == '\x7f') {
        const uint8_t erase[3] = "\b \b";
        if (erased) {
            tty->echo(tty, erase, 3);
            if (!is_printable(tty->line_buffer[tty->line_index])) {
                tty->echo(tty->data, erase, 3);
            }
        };
    } else if (tty->termios.c_lflag & ECHO) {
        if (is_printable(c) || c == '\n') {
            tty->echo(tty->data, (uint8_t*)&c, 1);
        } else {
            char caret[2];
            caret[0] = '^';
            caret[1] = c + 0x40;
            tty->echo(tty->data, (uint8_t*)caret, 2);
        }
    }
}

int tty_read(int tty_id, uint64_t offset, uint8_t *buffer, uint64_t len) {
    if (tty_id < 0) return -EINVAL;
    if (tty_id >= tty_count) return -EINVAL;
    tty_t* tty = ttys[tty_id];
    if (tty->read_head == tty->write_head) return -EAGAIN;
    uint64_t bytes_read = 0;
    for (bytes_read = 0; bytes_read < len; bytes_read++) {
        if (tty->read_head == tty->write_head) return bytes_read;
        buffer[bytes_read] = tty->read_buffer[tty->read_head++];
        tty->read_head %= 4096;
    }
    return bytes_read;
}

int tty_write(int tty_id, uint64_t offset, const uint8_t* buffer, uint64_t len) {
    if (tty_id < 0) return -EINVAL;
    if (tty_id >= tty_count) return -EINVAL;
    tty_t* tty = ttys[tty_id];
    if (tty->termios.c_oflag & OPOST) {
        for (int i = 0; i < len; i++) {
            uint8_t c = buffer[i];
            if (tty->termios.c_oflag & ONLCR && c == '\n') {
                tty->write(tty->data, (uint8_t*)"\r\n", 2);
            } else if (tty->termios.c_oflag & OCRNL && c == '\r') {
                tty->write(tty->data, (uint8_t*)"\n", 1);
            } else {
                tty->write(tty->data, &c, 1);
            }
        }
        return len;
    } else {
        return tty->write(tty->data, buffer, len);
    }
}

int tty_ioctl(int tty_id, uint64_t request, uint64_t arg) {
    if (tty_id < 0) return -EINVAL;
    if (tty_id >= tty_count) return -EINVAL;
    tty_t* tty = ttys[tty_id];
    void* data_ptr = (void*)arg;
    switch (request) {
        case TCGETS:
            if (!data_ptr) return -EINVAL;
            *(termios_t*)data_ptr = tty->termios;
            return 0;
        case TCSETS:
            if (!data_ptr) return -EINVAL;
            tty->termios = *(termios_t*)data_ptr;
            return 0;
        default:
            return -ENOTTY;
    }
}

void tty_init() {
    char_driver_t tty_driver = {
        .read = tty_read,
        .write = tty_write,
        .ioctl = tty_ioctl,
    };
    tty_driver_index = register_char_driver(&tty_driver);
    if (tty_driver_index < 0) tty_driver_index = 0;
}

int register_tty(tty_t *tty) {
    if (!tty) return -EINVAL;
    if (tty_count >= MAX_TTYS) return -EINVAL;
    ttys[tty_count] = tty;
    if (tty->name && tty_driver_index) {
        char_device_t dev = {
            .major_number = tty_driver_index,
            .minor_number = tty_count,
            .name = tty->name,
        };
        register_char_device(&dev);
    }
    return tty_count++;
}
