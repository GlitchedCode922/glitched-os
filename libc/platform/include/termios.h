#pragma once
#include <uapi/termios.h> // IWYU pragma: export

int tcgetattr(int fd, struct termios* p_termios);
int tcsetattr(int fd, struct termios* p_termios);
