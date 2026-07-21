#include <stddef.h>
#include <stdint.h>

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define O_CREAT 0x01
#define O_NONBLOCK 0x02
#define O_DIRECTORY 0x04

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

int64_t read(int fd, void* buffer, size_t size);
int64_t write(int fd, const void* buffer, size_t size);
int readdir(int fd, dirent_t* out);
int open(const char* path, uint16_t flags);
int close(int fd);
int64_t seek(int fd, int64_t offset, int type);
int64_t tell(int fd);
int dup(int fd);
int dup2(int fd, int new_fd);

int stat(const char* path, stat_t* out);
int link(const char* path, const char* link);
int remove_file(const char* path);
int create_file(const char* path);
int create_directory(const char* path);
int rename_file(const char* old_path, const char* new_path);
int chdir(char* path);
void getcwd(char* buffer, size_t size);

typedef int pid_t;

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
