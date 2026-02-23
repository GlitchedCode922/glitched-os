#include <stdio.h>
#include <unistd.h>

int main() {
    int console_fd = open_console(0);
    dup2(console_fd, STDIN_FILENO);
    dup2(console_fd, STDOUT_FILENO);
    dup2(console_fd, STDERR_FILENO);
    if (console_fd > STDERR_FILENO) {
        close(console_fd);
    }
    while (1) {
        pid_t sh = spawn("/bin/sh", (const char*[]){"/bin/sh", NULL});
        waitpid(sh, NULL, 0);
    }
}
