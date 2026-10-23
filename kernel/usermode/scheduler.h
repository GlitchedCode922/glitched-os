#pragma once
#include "usermode/fd.h"
#include "uapi/vvar.h" // IWYU pragma: export
#include "idt.h"
#include "vfs.h"
#include "uapi/signals.h" // IWYU pragma: export
#include "uapi/spawn.h" // IWYU pragma: export
#include <stdint.h>

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
    iframe_t signal_interrupted_iframe;
    int pid;
    int pgid;
    int sid;
    uint8_t in_signal;
    process_state_t state;
    void* cr3;
    void* initial_brk;
    void* brk;
    void* fpu_state;
    void* signal_interrupted_fpu_state;
    char wd[MAX_PATH];
    fd_t fd_table[MAX_FDS];
    sigaction_t signal_handlers[32];
    uint32_t pending_signals;
    uint32_t signal_mask;
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

typedef enum {
    SIGNAL_IGNORE,
    SIGNAL_TERMINATE,
} signal_action_t;

static const signal_action_t signal_default_actions[32] = {
    [SIGHUP]     = SIGNAL_TERMINATE,
    [SIGINT]     = SIGNAL_TERMINATE,
    [SIGQUIT]    = SIGNAL_TERMINATE,
    [SIGILL]     = SIGNAL_TERMINATE,
    [SIGTRAP]    = SIGNAL_TERMINATE,
    [SIGABRT]    = SIGNAL_TERMINATE,
    [SIGBUS]     = SIGNAL_TERMINATE,
    [SIGFPE]     = SIGNAL_TERMINATE,
    [SIGKILL]    = SIGNAL_TERMINATE,
    [SIGUSR1]    = SIGNAL_TERMINATE,
    [SIGSEGV]    = SIGNAL_TERMINATE,
    [SIGUSR2]    = SIGNAL_TERMINATE,
    [SIGPIPE]    = SIGNAL_TERMINATE,
    [SIGALRM]    = SIGNAL_TERMINATE,
    [SIGTERM]    = SIGNAL_TERMINATE,
    [SIGSTKFLT]  = SIGNAL_TERMINATE,
    [SIGCHLD]    = SIGNAL_IGNORE,
    [SIGCONT]    = SIGNAL_IGNORE,
    [SIGSTOP]    = SIGNAL_TERMINATE,
    [SIGTSTP]    = SIGNAL_TERMINATE,
    [SIGTTIN]    = SIGNAL_TERMINATE,
    [SIGTTOU]    = SIGNAL_TERMINATE,
    [SIGURG]     = SIGNAL_IGNORE,
    [SIGXCPU]    = SIGNAL_TERMINATE,
    [SIGXFSZ]    = SIGNAL_TERMINATE,
    [SIGVTALRM]  = SIGNAL_TERMINATE,
    [SIGPROF]    = SIGNAL_TERMINATE,
    [SIGWINCH]   = SIGNAL_IGNORE,
    [SIGIO]      = SIGNAL_TERMINATE,
    [SIGPWR]     = SIGNAL_TERMINATE,
    [SIGSYS]     = SIGNAL_TERMINATE
};

extern uint8_t scheduler_initialized;
extern task_t* current_task;
extern int64_t ticks_remaining;
extern uintptr_t vvar_paddr;
extern vvar_t* vvar_vaddr;

void scheduler_init();
void run_init(char* path);
void run_next(iframe_t* iframe);
void deliver_signals();
void exit(iframe_t* iframe, int ret);
vvar_t* get_vvar_addr();
int fork(iframe_t* iframe);
int spawn(char* path, char** argv, char** envp, spawn_action_t** actions, iframe_t* iframe);
int execve(char* path, char** argv, char** envp, iframe_t* iframe);
int create_kworker(void (*function)(void*), void* arg);
void yield_current();
void sleep_current(uint64_t ms);
void kworker_exit();
void sleep(uint64_t ms, iframe_t* iframe);
int waitpid(int pid, int* wstatus, int options, iframe_t* iframe);
int kill(int pid, int sig);
int raise(int sig);
int sigaction(int sig, const sigaction_t* new, sigaction_t* old);
void sigreturn();
int sigprocmask(int how, const uint32_t* set, uint32_t* old_set);
int getpid();
int getppid();
void check_blocked_tasks(int reduce_ticks);
int getpgid(int pid);
int setpgid(int pid, int pgid);
int getsid(int pid);
int setsid();
