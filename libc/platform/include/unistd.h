#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
#define O_CREAT 0x04
#define O_NONBLOCK 0x08
#define O_DIRECTORY 0x10
#define O_APPEND 0x20
#define O_CLOEXEC 0x40
#define O_EXCL 0x80

#define O_ACCESS 0x03

enum {
    DT_UNKNOWN = 0,
    DT_FILE = 1,
    DT_DIR = 2,
    DT_BLOCK = 3,
    DT_CHAR = 4,
};

typedef struct {
    char name[256];
    uint32_t type;
} __attribute__((packed)) dirent_t;

typedef struct {
    uint64_t size;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t btime;
    uint32_t type;
} __attribute__((packed)) stat_t;

ssize_t read(int fd, void* buffer, size_t size);
ssize_t write(int fd, const void* buffer, size_t size);
int readdir(int fd, dirent_t* out);
int open(const char* path, uint16_t flags, ...);
int close(int fd);
ssize_t lseek(int fd, off_t offset, int type);
ssize_t tell(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);

int stat(const char* path, stat_t* out);
int link(const char* path, const char* link);
int unlink(const char* path);
int mkdir(const char* path, ...);
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
