#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

int main() {
    mount(NULL, "/dev", "devfs", 0);
    mount("/dev/block/sda2", "/boot", "fat", 0);
    mount(NULL, "/tmp", "ramfs", 0);

    int console_fd = open("/dev/tty1", O_RDWR);
    dup2(console_fd, STDIN_FILENO);
    dup2(console_fd, STDOUT_FILENO);
    dup2(console_fd, STDERR_FILENO);
    if (console_fd > STDERR_FILENO) {
        close(console_fd);
    }

    termios_t term;
    if (tcgetattr(STDIN_FILENO, &term) < 0) return -1;
    term.c_iflag = ICRNL;
    term.c_oflag = OPOST;
    term.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOCTL;
    if (tcsetattr(STDIN_FILENO, &term) < 0) return -1;

    while (1) {
        pid_t sh = spawn(
            "/bin/sh",
            (char*[]){"/bin/sh", NULL},
            (char*[]){"PATH=/usr/bin:/bin", NULL},
            (spawn_action_t*[]){&(spawn_action_t){.action = SPAWN_SETSID}, NULL}
        );

        if (sh <= 0) {
            printf("init: failed to start shell\n");
            sleep(1);
            continue;
        }
        tcsetpgrp(STDIN_FILENO, sh);

        pid_t pid;

        do {
            pid = wait(NULL);
        } while (pid > 0 && pid != sh);

        if (pid < 0) {
            printf("init: wait failed\n");
            return 1;
        }
    }
}
