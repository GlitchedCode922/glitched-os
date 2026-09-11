#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int res = open(argv[1], O_CREAT | O_WRONLY);
    if (res < 0) {
        perror("Error creating file");
    }
    close(res);

    return 0;
}
