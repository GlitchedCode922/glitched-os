// Single-tasking/synchronous execve() and getenv() system call (until multitasking is implemented)
#pragma once
#include <stdint.h>
#include <stddef.h>

extern char** env;
extern char* wd;

int execve(const char* path, const char* argv[], const char* envp[]);
char* getenv(const char* name);
void init_exec(uintptr_t cr3);
