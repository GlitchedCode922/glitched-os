#include <stddef.h>
#include <stdint.h>

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define FLAG_CREATE 0x01

#define FLAG_NONBLOCKING 0x02

int read(int fd, void* buffer, size_t size);
int write(int fd, const void* buffer, size_t size);
int open_file(const char* path, uint16_t flags);
int open_framebuffer(uint16_t flags);
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

int isatty(int fd);

int mount(const char* source, const char* target, const char* type, int flags);
int umount(const char *path);
int umount_all();
