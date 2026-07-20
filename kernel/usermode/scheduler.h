#pragma once
#include <stdint.h>
#include "../idt.h"
#include "../vfs.h"
#include "fd.h"

typedef enum {
    STATE_READY,
    STATE_ZOMBIE,
    STATE_RUNNING,
    STATE_BLOCKED,
    STATE_DELETED
} process_state_t;

#define PROCESS_TICKS 10

#define WNOHANG 0x1

typedef enum {
    BLOCK_NONE,
    BLOCK_DELAY,
    BLOCK_WAITPID
} block_reason_t;

typedef struct Task {
    void* kernel_stack;
    iframe_t* iframe;
    int pid;
    process_state_t state;
    void* cr3;
    void* initial_brk;
    void* brk;
    void* fpu_state;
    char wd[MAX_PATH];
    fd_entry_t fd_table[MAX_FDS];
    fd_entry_t* fd_ptr_table[MAX_FDS];
    int64_t time_slice;
    block_reason_t block_reason;
    uint64_t blocked_ticks;
    int is_kworker;
    int* wstatus;
    struct Task* blocked_process;
    int return_code;
    struct Task* next;
    struct Task* next_sibling;
    struct Task* parent;
    struct Task* child;
} task_t;

extern uint8_t scheduler_initialized;
extern task_t* current_task;
extern int64_t ticks_remaining;

void scheduler_init();
void run_init(char* path);
void run_next(iframe_t* iframe);
void exit(int ret);
int fork(iframe_t* iframe);
int spawn(char* path, char** argv, char** envp, iframe_t* iframe);
int execve(char* path, char** argv, char** envp, iframe_t* iframe);
int create_kworker(void (*function)(void*), void* arg);
void yield_current();
void sleep_current(uint64_t ms);
void kworker_exit();
void sleep(uint64_t ms, iframe_t* iframe);
int waitpid(int pid, int* wstatus, int options, iframe_t* iframe);
int getpid();
int getppid();
void check_blocked_tasks(int reduce_ticks);
