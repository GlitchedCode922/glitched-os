#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <uapi/fd.h> // IWYU pragma: export
#include <uapi/dirent.h> // IWYU pragma: export

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

ssize_t read(int fd, void* buffer, size_t size);
ssize_t write(int fd, const void* buffer, size_t size);
int readdir(int fd, dirent_t* out);
int open(const char* path, uint16_t flags, ...);
int close(int fd);
ssize_t lseek(int fd, off_t offset, int type);
ssize_t tell(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);

int link(const char* path, const char* link);
int unlink(const char* path);
int mkdir(const char* path, ...);
int truncate(const char* path, size_t new_len);
int ftruncate(int fd, size_t new_len);
int chdir(char* path);
void getcwd(char* buffer, size_t size);

#define WNOHANG 0x1

pid_t fork();
pid_t spawn(const char *path, const char **argv, const char **envp);
int execve(const char *path, const char **argv, const char **envp);
pid_t waitpid(pid_t pid, int *wstatus, int options);
pid_t wait(int *wstatus);

void yield();
void sleep(uint64_t ms);

int isatty(int fd);

int mount(const char* source, const char* target, const char* type, int flags);
int umount(const char *path);

void* sbrk(intptr_t increment);
