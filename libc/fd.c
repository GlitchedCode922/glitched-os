#include "syscall.h"
#include "unistd.h"

int read(int path, void* buffer, size_t size) {
    return syscall(SYSCALL_READ, (uint64_t)path, (uint64_t)buffer, (uint64_t)size, 0, 0);
}

int write(int path, const void* buffer, size_t size) {
    return syscall(SYSCALL_WRITE, (uint64_t)path, (uint64_t)buffer, (uint64_t)size, 0, 0);
}

int open_file(const char* path, uint16_t flags) {
    return syscall(SYSCALL_OPEN_FILE, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

int open_console(uint16_t flags) {
    return syscall(SYSCALL_OPEN_CONSOLE, (uint64_t)flags, 0, 0, 0, 0);
}

int open_framebuffer(uint16_t flags) {
    return syscall(SYSCALL_OPEN_FRAMEBUFFER, (uint64_t)flags, 0, 0, 0, 0);
}

int open_serial(int port, uint16_t flags) {
    return syscall(SYSCALL_OPEN_SERIAL, port, (uint64_t)flags, 0, 0, 0);
}

int close(int fd) {
    return syscall(SYSCALL_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

int seek(int fd, int64_t offset, int type) {
    return syscall(SYSCALL_SEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)type, 0, 0);
}

int dup(int fd) {
    return syscall(SYSCALL_DUP, (uint64_t)fd, 0, 0, 0, 0);
}

int dup2(int fd, int new_fd) {
    return syscall(SYSCALL_DUP2, (uint64_t)fd, (uint64_t)new_fd, 0, 0, 0);
}
