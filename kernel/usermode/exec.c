#include "exec.h"
#include "elf.h"
#include "../memory/paging.h"
#include <stdint.h>

char** env = {NULL};
void* base_pml4 = NULL;

void init_exec(uintptr_t cr3) {
    // Initialize the base PML4 address for user mode execution
    base_pml4 = clone_page_tables((void*)cr3);
}

int execve(const char *path, const char **argv, const char **envp) {
    void* old_pml4;
    asm volatile("mov %%cr3, %0" : "=r"(old_pml4) : : "memory");
    asm volatile("mov %0, %%cr3" : : "r"(base_pml4) : "memory");
    void* new_pml4 = clone_page_tables(base_pml4);
    if (new_pml4 == NULL) {
        return -1; // Failed to clone page tables
    }
    asm volatile("mov %0, %%cr3" : : "r"(new_pml4) : "memory");

    // Check if the path is NULL
    if (path == NULL) {
        return -1; // Invalid path
    }

    // Check if the executable exists and is valid
    if (!is_compatible_binary(path)) {
        return -2; // Executable not found or invalid
    }

    // Set up the environment variables
    char** parent_env = env;
    env = (char**)envp;

    // Find argument count
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }

    // Load the executable into memory
    void* entry_point = load_elf(path);
    if (entry_point == 0) {
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

    return result; // Success
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
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
