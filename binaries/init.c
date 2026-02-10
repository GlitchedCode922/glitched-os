#include <stdio.h>
#include <exec.h>

int main() {
    int console_fd = open_console();
    dup2(console_fd, STDIN_FILENO);
    dup2(console_fd, STDOUT_FILENO);
    dup2(console_fd, STDERR_FILENO);
    if (console_fd > STDERR_FILENO) {
        close(console_fd);
    }
    exec("/bin/sh", (const char*[]){"/bin/sh", NULL}, (const char*[]){NULL});
}
