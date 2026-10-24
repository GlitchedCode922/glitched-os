#include <stdlib.h>
#include <uapi/vvar.h>
#include <syscall.h>

char** environ;
int environ_copied = 0;
vvar_t* vvar;
extern int main(int argc, char* argv[], char* envp[]);

int _libc_init_main(int argc, char* argv[], char* envp[]) {
    environ = envp;
    vvar = (vvar_t*)syscall(SYSCALL_GET_VVAR_ADDR, 0, 0, 0, 0, 0, 0);
    exit(main(argc, argv, envp));
}
