#include <stdio.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int res = remove_file(argv[1]);
    if (res < 0) {
        perror("Failed to remove file");
        return 1;
    }
    return 0;
}
