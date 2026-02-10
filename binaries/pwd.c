#include <stdio.h>

int main() {
    char cwd[4096] = {0};
    getcwd(cwd);
    printf("%s\n", cwd);
    return 0;
}
