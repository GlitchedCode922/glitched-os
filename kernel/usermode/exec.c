#include "exec.h"
#include "elf.h"
#include "../memory/paging.h"
#include "../memory/mman.h"
#include "../mount.h"
#include "break.h"
#include "fd.h"
#include <stdint.h>

char* env_arr[] = {NULL};
char** env = env_arr;
void* base_pml4 = NULL;
char init_wd[4096] = "/";
char* wd = init_wd;

void init_exec(uintptr_t cr3) {
    // Initialize the base PML4 address for user mode execution
    base_pml4 = clone_page_tables((void*)cr3);
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char* strcpy(char *dest, const char *src) {
    char *ptr = dest;
    while ((*ptr++ = *src++) != '\0');
    return dest;
}

int execve(const char *path, const char **argv, const char **envp) {
    // --- Clone argv ---
    uint64_t argc = 0;
    while (argv[argc]) argc++;

    char* new_path = kmalloc(1024);
    if (new_path == NULL) {
        return -1; // Memory allocation failed
    }
    // Copy the path string
    uint64_t path_len = 0;
    while (*path != '\0') new_path[path_len++] = *path++;
    new_path[path_len] = '\0'; // Null-terminate the new path
    path = new_path;

    // Allocate new pointer array (+1 for NULL)
    char** new_argv = kmalloc((argc + 1) * sizeof(char*));

    // Copy strings
    for (uint64_t i = 0; i < argc; i++) {
        const char* src = argv[i];

        // Count string length manually
        uint64_t len = 0;
        while (src[len] != '\0') len++;

        // Allocate new string and copy
        char* dst = kmalloc(len + 1);
        for (uint64_t j = 0; j <= len; j++) {
            dst[j] = src[j];
        }

        new_argv[i] = dst;
    }
    new_argv[argc] = 0;

    // --- Clone envp ---
    uint64_t envc = 0;
    while (envp[envc]) envc++;

    char** new_envp = kmalloc((envc + 1) * sizeof(char*));

    for (uint64_t i = 0; i < envc; i++) {
        const char* src = envp[i];

        uint64_t len = 0;
        while (src[len] != '\0') len++;

        char* dst = kmalloc(len + 1);
        for (uint64_t j = 0; j <= len; j++) {
            dst[j] = src[j];
        }

        new_envp[i] = dst;
    }
    new_envp[envc] = 0;

    argv = (const char**)new_argv;
    envp = (const char**)new_envp;

    void* initial_brk_ptr = initial_brk;
    void* break_ptr = brk;

    // Save current file descriptors
    fd_entry_t parent_fds[MAX_FDS];
    for (int i = 0; i < MAX_FDS; i++) {
        parent_fds[i] = fd_table[i];
    }

    // Replace PML4
    void* old_pml4;
    asm volatile("mov %%cr3, %0" : "=r"(old_pml4) : : "memory");
    asm volatile("mov %0, %%cr3" : : "r"(base_pml4) : "memory");
    void* new_pml4 = clone_page_tables(base_pml4);
    if (new_pml4 == NULL) {
        // Restore the old PML4
        asm volatile("mov %0, %%cr3" : : "r"(old_pml4) : "memory");
        change_pml4(old_pml4);
        return -1; // Failed to clone page tables
    }
    asm volatile("mov %0, %%cr3" : : "r"(new_pml4) : "memory");
    change_pml4(new_pml4);

    // Check if the path is NULL
    if (path == NULL) {
        // Restore the old PML4
        asm volatile("mov %0, %%cr3" : : "r"(old_pml4) : "memory");
        change_pml4(old_pml4);
        return -1; // Invalid path
    }

    // Check if the executable exists and is valid
    if (!is_compatible_binary(path)) {
        // Restore the old PML4
        asm volatile("mov %0, %%cr3" : : "r"(old_pml4) : "memory");
        change_pml4(old_pml4);
        return -2; // Executable not found or invalid
    }

    // Set up the environment variables
    char** parent_env = env;
    env = (char**)envp;

    char* parent_wd = wd;
    wd = kmalloc(4096);
    strcpy(wd, parent_wd);

    // Load the executable into memory
    void* entry_point = load_elf(path, &initial_brk);
    brk = initial_brk;

    alloc_page((uintptr_t)initial_brk, FLAGS_USER | FLAGS_RW);
    if (entry_point == 0) {
        // Restore the old PML4
        asm volatile("mov %0, %%cr3" : : "r"(old_pml4) : "memory");
        change_pml4(old_pml4);
        // Restore the parent environment
        env = parent_env;
        kfree(wd);
        wd = parent_wd;
        return -3; // Failed to load executable
    }

    // Run the entry point of the executable
    uint64_t (*entry)(int argc, char** argv) = (uint64_t (*)(int, char**))entry_point;
    uint64_t result = entry(argc, (char**)argv);

    // Restore the old PML4
    asm volatile("mov %0, %%cr3" : : "r"(old_pml4) : "memory");
    change_pml4(old_pml4);

    // Restore the parent environment
    env = parent_env;
    kfree(wd);
    wd = parent_wd;

    // Restore file descriptors
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i] = parent_fds[i];
    }

    initial_brk = initial_brk_ptr;
    brk = break_ptr;

    return result; // Success
}

char* getenv(const char *name) {
    // Check if the environment is set
    if (env == NULL) {
        return 0; // No environment variables set
    }

    // Search for the environment variable
    for (int i = 0; env[i] != NULL; i++) {
        const char* var = env[i];
        // Find the '=' character
        const char* eq = var;
        while (*eq != '=' && *eq != '\0') {
            eq++;
        }

        // If we found the '=' and the name matches
        if (*eq == '=' && strcmp(eq, name) == 0) {
            return (char*)(eq + 1); // Return the value after '='
        }
    }

    return 0; // Variable not found
}
