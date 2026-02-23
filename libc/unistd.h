#include <stddef.h>
#include <stdint.h>

int read(int fd, void* buffer, size_t size);
int write(int fd, const void* buffer, size_t size);
int open_file(const char* path, uint16_t flags);
int open_console(uint16_t flags);
int open_framebuffer(uint16_t flags);
int open_serial(int port, uint16_t flags);
int close(int fd);
int seek(int fd, int64_t offset, int type);
int dup(int fd);
int dup2(int fd, int new_fd);

typedef int pid_t;

#define WNOHANG 0x1

pid_t fork();
pid_t spawn(const char *path, const char **argv);
int execv(const char *path, const char **argv);
pid_t waitpid(pid_t pid, int *wstatus, int options);
pid_t wait(int *wstatus);

void yield();
void sleep(uint64_t ms);
