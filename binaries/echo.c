#include <stdio.h>

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("\n");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (argc - 1 != i) {
            printf(" ");
        }
    }

    printf("\n");
    return 0;
}
