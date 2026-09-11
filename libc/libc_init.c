#include <stdlib.h>

char** environ;
extern int main(int argc, char* argv[], char* envp[]);

int _libc_init_main(int argc, char* argv[], char* envp[]) {
    environ = envp;
    exit(main(argc, argv, envp));
}
