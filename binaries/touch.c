#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Invalid argument count: %d\n", argc - 1);
        return 2;
    }

    int res = create_file(argv[1]);
    if (res < 0) {
        perror("Error creating file");
    }

    return 0;
}
