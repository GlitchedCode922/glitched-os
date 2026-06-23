#include "termios.h"
#include "ioctl.h"
#include "unistd.h"

int tcgetattr(int fd, struct termios *p_termios) {
    return ioctl(fd, TCGETS, p_termios);
}

int tcsetattr(int fd, struct termios *p_termios) {
    return ioctl(fd, TCSETS, p_termios);
}

int isatty(int fd) {
    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        return 0;
    } else {
        return 1;
    }
}
