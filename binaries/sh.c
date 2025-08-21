#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <exec.h>

int main() {
    while (1) {
        printf("$ ");
        char* command = read_console();
        char *args[512] = {0};
        int arg_idx = 0;
        while (*command != 0) {
            args[arg_idx++] = command;
            while (*command != ' ' && *command != 0 && *command != '\n') {
                command++;
            }
            if (*command) *command++ = '\0';
        }
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                printf("Missing argument for cd\n");
                continue;
            }
            int ret = chdir(args[1]);
            if (ret != 0) {
                printf("Error: %d\n", ret);
            }
            continue;
        }

        if (strchr(args[0], '/') == NULL) {
            // Merge args[0] with /bin
            char program_in_bin[strlen("/bin/") + strlen(args[0])];
            strcpy(program_in_bin, "/bin/");
            strcat(program_in_bin, args[0]);

            int ret = exec(program_in_bin, (const char**)args, (const char*[]){NULL});
            if (ret != 0) {
                printf("Error: %d\n", ret);
            }
            continue;
        }
        
        int ret = exec(args[0], (const char**)args, (const char*[]){NULL});
        if (ret != 0) {
            printf("Error: %d\n", ret);
        }
    }
}

int _start() {
    return main();
}
