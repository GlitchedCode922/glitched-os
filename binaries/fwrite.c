#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    FILE* fp = fopen(argv[1], "w");
    if (fp == NULL) {
        perror(argv[1]);
        return 1;
    }
    char console_line[256];
    while (1) {
        fgets(console_line, sizeof(console_line), stdin);
        if (strcmp(console_line, "\\exit\n") == 0) return 0;
        int res = fwrite(console_line, 1, strlen(console_line), fp);
        if (res < 0) {
            perror("Write error");
            return 1;
        }
    }
    fclose(fp);
}
