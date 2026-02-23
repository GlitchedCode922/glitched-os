#include <stdio.h>

int main() {
    char cwd[4096] = {0};
    getcwd(cwd, 4096);
    printf("%s\n", cwd);
    return 0;
}
