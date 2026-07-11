#include <stdio.h>
#include <unistd.h>

int main() {
    mount(NULL, "/dev", "devfs", 0);
    mount("/dev/sda2", "/boot", "FAT", 0);
    mount(NULL, "/tmp", "ramfs", 0);
    int console_fd = open("/dev/tty1", 0);
    dup2(console_fd, STDIN_FILENO);
    dup2(console_fd, STDOUT_FILENO);
    dup2(console_fd, STDERR_FILENO);
    if (console_fd > STDERR_FILENO) {
        close(console_fd);
    }
    pid_t sh = spawn("/bin/sh", (const char*[]){"/bin/sh", NULL});
    if (sh <= 0) printf("Error running shell at /bin/sh");
    while (1) {
        wait(NULL);
    }
}
