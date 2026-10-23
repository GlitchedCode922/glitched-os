#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

int main(int argc, char** argv, char** envp) {
    setsid();
    termios_t old_termios;
    termios_t new_termios;
    int res = tcgetattr(STDIN_FILENO, &old_termios);
    if (res < 0) {
        perror("tcgetattr");
        return 1;
    }
    new_termios = old_termios;
    new_termios.c_lflag |= ICANON | ISIG | ECHO | ECHOE | ECHOCTL;
    res = tcsetattr(STDIN_FILENO, &new_termios);
    if (res < 0) {
        perror("tcsetattr");
        return 1;
    }
    while (1) {
        printf("$ ");
        fflush(stdout);
        char command_buffer[1024] = {0};
        char* res = fgets(command_buffer, sizeof(command_buffer), stdin);
        if (res == NULL) return 0;
        char *command = command_buffer;
        char *args[512] = {0};
        int arg_idx = 0;
        while (*command != 0) {
            args[arg_idx++] = command;
            while (*command != ' ' && *command != 0 && *command != '\n') {
                command++;
            }
            if (*command) *command++ = '\0';
        }

        // Search for empty args and remove them shifting the rest
        for (int i = 0; i < arg_idx; i++) {
            if (args[i][0] == 0) {
                for (int j = i; j < arg_idx - 1; j++) {
                    args[j] = args[j + 1];
                }
                args[arg_idx - 1] = NULL;
                arg_idx--;
                i--;
            }
        }

        if (args[0] == NULL) continue; // Empty command

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                int ret = chdir("/");
                if (ret != 0) {
                    perror("Error");
                }
                continue;
            }
            int ret = chdir(args[1]);
            if (ret != 0) {
                perror("Error");
            }
            continue;
        }

        int ret;
        pid_t p = spawn_p(args[0], args, envp, NULL);
        if (p > 0) {
            setpgid(p, p);
            tcsetpgrp(STDIN_FILENO, p);
            waitpid(p, &ret, 0);
            tcsetpgrp(STDIN_FILENO, 0);
            if (ret != 0) {
                printf("Command exited with error code %d\n", ret);
            }
        } else {
            perror("Error");
        }
    }
    res = tcsetattr(STDIN_FILENO, &old_termios);
    if (res < 0) {
        perror("tcsetattr");
        return 1;
    }
}
