#include "scheduler.h"
#include "../vfs.h"
#include "fd.h"
#include "syscalls.h"
#include "../gdt.h"
#include "../memory/mman.h"
#include "elf.h"
#include "../memory/paging.h"
#include "../panic.h"
#include "../drivers/fpu.h"
#include "../error.h"
#include <stdint.h>

extern void jump_to_user(void* rip, void* rsp);
extern void context_switch(void* iframe);

task_t idle_task = {.pid = 0, .next = &idle_task, .time_slice = PROCESS_TICKS, .wd = "/"};
task_t init_task = {.pid = 1, .next = &init_task, .time_slice = PROCESS_TICKS, .wd = "/"};
task_t* current_task = &init_task;
task_t* rr_task = &init_task;
int last_pid = 1;
uint8_t scheduler_initialized = 0;
int64_t ticks_remaining = PROCESS_TICKS;
void* base_pml4;

void idle() {
    while (1) asm volatile("hlt");
}

void gc_tasks() {
    task_t* p = &init_task;
    task_t* t = init_task.next;
    while (t != &init_task) {
        if (t->state == STATE_DELETED) {
            free_page_tables(t->cr3);
            kfree(t->kernel_stack - 4096 * 32);
            kfree(t->fpu_state);
            p->next = t->next;
            if (!t->is_kworker) {
                if (t->parent->child == t) {
                    t->parent->child = t->next_sibling;
                } else {
                    task_t* s = t->parent->child;
                    while (s->next_sibling) {
                        if (s->next_sibling == t) {
                            s->next_sibling = t->next_sibling;
                            break;
                        }
                        s = s->next_sibling;
                    }
                }
            }
            task_t* next = t->next;
            if (current_task == t) current_task = next;
            if (rr_task == t) rr_task = next;
            kfree(t);
            t = next;
            continue;
        }
        p = t;
        t = p->next;
    }
}

void scheduler_init() {
    asm volatile(
        "mov %%cr3, %0"
        : "=r"(base_pml4)
    );
    idle_task.cr3 = clone_page_tables(base_pml4);
    idle_task.kernel_stack = kmalloc(4096 * 32) + 4096 * 32;
    idle_task.fpu_state = kmalloc(fpu_memory_size);
    idle_task.state = STATE_READY;
    idle_task.iframe = (iframe_t*)(idle_task.kernel_stack - sizeof(iframe_t));
    idle_task.iframe->rip = (uint64_t)idle;
    idle_task.iframe->rflags = 0x200;
    idle_task.iframe->cs = KERNEL_CS;
    idle_task.iframe->rsp = (uint64_t)idle_task.kernel_stack - sizeof(iframe_t);
    idle_task.iframe->ss = 0x10;
    idle_task.is_kworker = 1;
}

task_t* get_next_task() {
    task_t* start = rr_task;
    do {
        rr_task = rr_task->next;
        if (rr_task->state == STATE_READY) return rr_task;
    } while (rr_task != start);
    return NULL;
}

void run_init(char* path) {
    init_task.cr3 = clone_page_tables(base_pml4);
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(init_task.cr3)
    );
    change_pml4(init_task.cr3);
    file_handle_t file;
    int res = lookup(path, &file);
    if (res < 0) panic("Failed to load init binary");
    void* addr = load_elf(file, &init_task.initial_brk);
    if (!addr) {
        panic("Failed to load init binary: %s", path);
    }
    init_task.parent = &init_task;
    init_task.brk = init_task.initial_brk;
    alloc_region(0x10000000000, 4096 * 128, FLAGS_PRESENT | FLAGS_RW | FLAGS_USER);
    void* kstack = (char*)kmalloc(4096 * 32) + 4096 * 32;
    init_task.kernel_stack = kstack;
    tss.rsp0 = (uint64_t)kstack;
    init_task.fpu_state = kmalloc(fpu_memory_size);
    current_task = &init_task;
    scheduler_initialized = 1;
    jump_to_user(addr, (char*)0x10000000000 + 4096 * 128 - 16);
}

static void switch_task() {
    task_t* next_task = get_next_task();
    if (!next_task) {
        next_task = &idle_task;
    }
    current_task = next_task;
    current_task->state = STATE_RUNNING;
    ticks_remaining = current_task->time_slice;
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(current_task->cr3)
    );
    change_pml4(current_task->cr3);
    restore_fpu(current_task->fpu_state);
    tss.rsp0 = (uint64_t)current_task->kernel_stack;
    current_task->iframe->rflags |= 0x200;
    context_switch(current_task->iframe);
}

void run_next(iframe_t* iframe) {
    if (!scheduler_initialized) return;
    current_task->iframe = iframe;
    save_fpu(current_task->fpu_state);
    if (current_task->state == STATE_RUNNING) current_task->state = STATE_READY;
    gc_tasks();
    switch_task();
}

void exit(int ret) {
    if (current_task == &init_task) panic("Init process exited!");
    release_process_fds();
    current_task->state = STATE_ZOMBIE;
    current_task->return_code = ret;

    // Reparent children
    task_t* c = current_task->child;
    while (c) {
        task_t* next = c->next_sibling;
        c->parent = &init_task;
        c->next_sibling = init_task.child;
        init_task.child = c;
        c = next;
    }

    switch_task();
}

int fork(iframe_t* iframe) {
    if (last_pid == 2147483647) panic("No PIDs available");
    current_task->iframe = iframe;
    task_t* new_task = kmalloc(sizeof(task_t));
    *new_task = *current_task;
    new_task->state = STATE_READY;
    new_task->cr3 = clone_page_tables(current_task->cr3);
    void* kstack = (char*)kmalloc(4096 * 32) + 4096 * 32;
    iframe_t* new_iframe = kstack - sizeof(iframe_t);
    *new_iframe = *current_task->iframe;
    new_task->kernel_stack = kstack;
    new_task->iframe = new_iframe;
    new_task->fpu_state = kmalloc(fpu_memory_size);
    new_task->next = current_task->next;
    current_task->next = new_task;
    new_task->next_sibling = current_task->child;
    new_task->child = NULL;
    new_task->pid = ++last_pid;
    new_task->parent = current_task;
    current_task->child = new_task;
    current_task->iframe->rax = new_task->pid;
    new_task->iframe->rax = 0;
    return new_task->pid;
}

static int strlen(char* s) {
    int len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

int add_task(char* path, char** argv, task_t* parent, int pid, iframe_t* iframe) {
    // Copy path to kernel memory
    size_t path_len = strlen(path) + 1;
    char kpath[path_len];
    memcpy(kpath, path, path_len);

    // Count arguments
    int argc = 0;
    while (argv[argc]) argc++;

    // Copy argv strings to kernel memory
    char* kargv[sizeof(char*) * (argc + 1)];
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        kargv[i] = kmalloc(len);
        memcpy(kargv[i], argv[i], len);
    }
    kargv[argc] = NULL;

    task_t* new_task = kmalloc(sizeof(task_t));
    *new_task = *parent;
    new_task->state = STATE_READY;
    new_task->cr3 = clone_page_tables(base_pml4);

    // Clone FD file handles
    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (new_task->fd_table[fd].refcount == 0) continue;
        int res = clone_file_handle(new_task->fd_table[fd].file_handle);
        if (res < 0) return res;
    }

    // Switch to the new page table
    asm volatile("mov %0, %%cr3" :: "r"(new_task->cr3));
    change_pml4(new_task->cr3);

    // Load the ELF
    stat_t st;
    int res = stat(kpath, &st);
    if (res < 0) {
        asm volatile("mov %0, %%cr3" :: "r"(current_task->cr3));
        change_pml4(current_task->cr3);
        return res;
    } else if (st.type == DT_DIR) {
        asm volatile("mov %0, %%cr3" :: "r"(current_task->cr3));
        change_pml4(current_task->cr3);
        return -EISDIR;
    }

    file_handle_t file;
    res = lookup(kpath, &file);
    if (res < 0) return res;

    if (!is_compatible_binary(file)) {
        // Restore old page table
        asm volatile("mov %0, %%cr3" :: "r"(current_task->cr3));
        change_pml4(current_task->cr3);
        return -ENOEXEC;
    }

    void* entry = load_elf(file, &new_task->initial_brk);
    if (!entry) {
        // Restore old page table
        asm volatile("mov %0, %%cr3" :: "r"(current_task->cr3));
        change_pml4(current_task->cr3);
        return -ENOEXEC;
    }

    // Allocate user stack
    alloc_region(0x10000000000, 4096 * 128, FLAGS_PRESENT | FLAGS_RW | FLAGS_USER);
    uintptr_t user_stack = 0x10000000000 + 4096 * 128;

    // Temporary array for string addresses on user stack
    uintptr_t argv_ptrs[sizeof(uintptr_t) * argc];

    // Copy argv strings onto user stack
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(kargv[i]) + 1;
        user_stack -= len;
        memcpy((void*)user_stack, kargv[i], len);
        argv_ptrs[i] = user_stack;
        kfree(kargv[i]);
    }

    // Push argv pointers
    for (int i = argc; i >= 0; i--) {
        user_stack -= sizeof(uintptr_t);
        *(uintptr_t*)user_stack = (i < argc) ? argv_ptrs[i] : 0;
    }
    uintptr_t argv_start = user_stack;

    // Push argc
    user_stack -= 8;
    *(uint64_t*)user_stack = argc;

    // Setup kernel stack and iframe
    void* kstack = (char*)kmalloc(4096 * 32) + 4096 * 32;
    iframe_t* new_iframe = kstack - sizeof(iframe_t);
    *new_iframe = *iframe;
    new_iframe->rsp = user_stack;
    new_iframe->rip = (uint64_t)entry;
    new_task->kernel_stack = kstack;
    new_task->iframe = new_iframe;
    new_task->fpu_state = kmalloc(fpu_memory_size);

    // Link task tree
    new_task->next = parent->next;
    parent->next = new_task;
    new_task->next_sibling = parent->child;
    new_task->child = NULL;
    new_task->pid = pid;
    new_task->parent = parent;
    parent->child = new_task;

    return pid;
}

int spawn(char* path, char** argv, iframe_t* iframe) {
    if (last_pid == 2147483647) panic("No PIDs available");

    current_task->iframe = iframe;
    return add_task(path, argv, current_task, ++last_pid, iframe);
}

int execv(char *path, char **argv, iframe_t *iframe) {
    int res = add_task(path, argv, current_task->parent, current_task->pid, iframe);
    if (res != current_task->pid) {
        return res;
    }
    current_task->state = STATE_DELETED;

    switch_task();
    return 0; // Suppress compiler warning
}

void kworker_trampoline(void (*fn)(void*), void* arg) {
    fn(arg);
    kworker_exit();
}

int create_kworker(void (*function)(void*), void* arg) {
    if (last_pid == 2147483647) panic("No PIDs available");
    iframe_t iframe = {0};
    iframe.rip = (uint64_t)kworker_trampoline;
    iframe.rdi = (uint64_t)function;
    iframe.rsi = (uint64_t)arg;
    iframe.rflags = 0x200;
    iframe.cs = KERNEL_CS;
    int pid = ++last_pid;
    task_t* new_task = kmalloc(sizeof(task_t));
    *new_task = init_task;
    new_task->state = STATE_READY;
    new_task->cr3 = clone_page_tables(base_pml4);
    void* kstack = (char*)kmalloc(4096 * 32) + 4096 * 32;
    iframe.rsp = (uint64_t)kstack;
    iframe.ss = 0x10;
    iframe_t* new_iframe = kstack - sizeof(iframe_t);
    *new_iframe = iframe;
    new_task->kernel_stack = kstack;
    new_task->iframe = new_iframe;
    new_task->fpu_state = kmalloc(fpu_memory_size);
    new_task->is_kworker = 1;
    new_task->next = current_task->next;
    current_task->next = new_task;
    new_task->child = NULL;
    new_task->next_sibling = NULL;
    new_task->pid = pid;
    new_task->parent = NULL;
    return pid;
}

void yield_current() {
    asm volatile(
        "movq %0, %%rax\n\t"
        "int $0x80\n\t"
        :
        : "r"((uint64_t)SYSCALL_YIELD)
        : "rax"
    );
}

void sleep_current(uint64_t ms) {
    asm volatile(
        "movq %0, %%rdi\n\t"
        "movq %1, %%rax\n\t"
        "int $0x80\n\t"
        :
        : "r"(ms), "r"((uint64_t)SYSCALL_SLEEP)
        : "rax", "rdi"
    );
}

void kworker_exit() {
    if (!current_task->is_kworker) return;
    current_task->state = STATE_DELETED;
    switch_task();
}

void sleep(uint64_t ms, iframe_t *iframe) {
    if (ms == 0) return;
    current_task->state = STATE_BLOCKED;
    current_task->block_reason = BLOCK_DELAY;
    current_task->blocked_ticks = ms;
    current_task->iframe = iframe;
    switch_task();
}

task_t* get_child(task_t* task, int pid) {
    task_t* c = task->child;
    while (c) {
        if (c->state != STATE_DELETED && c->pid == pid) return c;
        c = c->next_sibling;
    }
    return NULL;
}

task_t* get_first_zombie(task_t* task) {
    task_t* c = task->child;
    while (c) {
        if (c == NULL) return NULL;
        if (c->state == STATE_ZOMBIE) return c;
        c = c->next_sibling;
    }
    return NULL;
}

int waitpid(int pid, int* wstatus, int options, iframe_t* iframe) {
    if (pid > 0) {
        task_t* child = get_child(current_task, pid);
        if (child == NULL) return -EINVAL;
        if (child->state == STATE_ZOMBIE) {
            if (wstatus) *wstatus = child->return_code;
            child->state = STATE_DELETED;
            return pid;
        }
        if (options & WNOHANG) return 0;
        current_task->state = STATE_BLOCKED;
        current_task->block_reason = BLOCK_WAITPID;
        current_task->blocked_process = child;
        current_task->wstatus = wstatus;
        run_next(iframe);
    } else {
        task_t* child = get_first_zombie(current_task);
        if (child != NULL) {
            if (wstatus) *wstatus = child->return_code;
            child->state = STATE_BLOCKED;
            return child->pid;
        }
        if (options & WNOHANG) return -1;
        current_task->state = STATE_BLOCKED;
        current_task->block_reason = BLOCK_WAITPID;
        current_task->blocked_process = NULL;
        current_task->wstatus = wstatus;
        run_next(iframe);
    }

    return -1; // Unreachable
}

void check_blocked_tasks(int reduce_ticks) {
    task_t* task = &init_task;
    do {
        if (task->state == STATE_BLOCKED) {
            if (task->block_reason == BLOCK_DELAY) {
                if (reduce_ticks) task->blocked_ticks--;
                if (task->blocked_ticks == 0) {
                    task->state = STATE_READY;
                    task->block_reason = BLOCK_NONE;
                }
            } else if (task->block_reason == BLOCK_WAITPID) {
                if (task->blocked_process != NULL) {
                    if (task->blocked_process->state == STATE_ZOMBIE) {
                        asm volatile(
                            "mov %0, %%cr3"
                            :: "r"(task->cr3)
                        );
                        if (task->wstatus) *task->wstatus = task->blocked_process->return_code;
                        asm volatile(
                            "mov %0, %%cr3"
                            :: "r"(current_task->cr3)
                        );
                        task->blocked_process->state = STATE_DELETED;
                        task->state = STATE_READY;
                        task->block_reason = BLOCK_NONE;
                        task->iframe->rax = task->blocked_process->pid;
                        task->blocked_process = NULL;
                    }
                } else {
                    task_t* blocked = get_first_zombie(task);
                    if (blocked == NULL) {
                        task = task->next;
                        continue;
                    }
                    asm volatile(
                        "mov %0, %%cr3"
                        :: "r"(task->cr3)
                    );
                    if (task->wstatus) *task->wstatus = blocked->return_code;
                    asm volatile(
                        "mov %0, %%cr3"
                        :: "r"(current_task->cr3)
                    );
                    blocked->state = STATE_DELETED;
                    task->state = STATE_READY;
                    task->block_reason = BLOCK_NONE;
                    task->iframe->rax = blocked->pid;
                }
            }
        }
        task = task->next;
    } while (task != &init_task);
}

int getpid() {
    return current_task->pid;
}

int getppid() {
    if (current_task == &init_task) return 0;
    return current_task->parent->pid;
}
