#include "stdio.h"
#include "syscall.h"
#include <stdint.h>

int read_file(const char* path, void* buffer, int offset, size_t size) {
    return syscall(SYSCALL_READ_FILE, (uint64_t)path, (uint64_t)buffer, offset, size, 0);
}

int write_file(const char* path, const void* buffer, int offset, size_t size) {
    return syscall(SYSCALL_READ_FILE, (uint64_t)path, (uint64_t)buffer, offset, size, 0);
}

uint64_t get_file_size(const char* path) {
    return syscall(SYSCALL_GET_FILE_SIZE, (uint64_t)path, 0, 0, 0, 0);
}

int list_directory(const char *path, char *element, uint64_t element_index) {
    return syscall(SYSCALL_LIST_DIR, (uint64_t)path, (uint64_t)element, element_index, 0, 0);
}

int file_exists(const char *path) {
    return syscall(SYSCALL_FILE_EXISTS, (uint64_t)path, 0, 0, 0, 0);
}

int is_directory(const char *path) {
    return syscall(SYSCALL_IS_DIRECTORY, (uint64_t)path, 0, 0, 0, 0);
}

void remove_file(const char* path) {
    syscall(SYSCALL_DELETE_FILE, (uint64_t)path, 0, 0, 0, 0);
}

void create_file(const char* path) {
    syscall(SYSCALL_CREATE_FILE, (uint64_t)path, 0, 0, 0, 0);
}

void create_directory(const char* path) {
    syscall(SYSCALL_CREATE_DIR, (uint64_t)path, 0, 0, 0, 0);
}

void getcwd(char *buffer) {
    syscall(SYSCALL_GETCWD, (uint64_t)buffer, 0, 0, 0, 0);
}

int chdir(char *path) {
    return syscall(SYSCALL_CHDIR, (uint64_t)path, 0, 0, 0, 0);
}
