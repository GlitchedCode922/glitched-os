#include "scheduler.h"
#include "../user_jump.h"
#include "../gdt.h"
#include "../memory/mman.h"
#include "elf.h"
#include "../memory/paging.h"
#include "../panic.h"
#include <stdint.h>

task_t init_task = {.pid = 1, .next = &init_task, .time_slice = PROCESS_TICKS, .wd = "/"};
task_t* current_task = &init_task;
int last_pid = 1;
uint8_t scheduler_initialized = 0;
int64_t ticks_remaining = PROCESS_TICKS;
void* base_pml4;

void gc_tasks() {
    task_t* p = &init_task;
    task_t* t = init_task.next;
    while (t != &init_task) {
        if (t->state == STATE_DELETED) {
            kfree(t->kernel_stack - 4096 * 32);
            p->next = t->next;
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
            task_t* next = t->next;
            kfree(t);
            t = next;
            continue;
        }
        p = t;
        t = p->next;
    }
}

void cleanup_memory(task_t* task) {
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(task->cr3)
    );
    change_pml4(task->cr3);
    free_region(0x400000, (size_t)(task->brk - 0x400000));
    free_region(0x10000000000, 4096 * 128);
    task->state = STATE_DELETED; // Mark as deleted for garbage collection
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(current_task->cr3)
    );
    change_pml4(current_task->cr3);
}

void run_init(char* path) {
    asm volatile(
        "mov %%cr3, %0"
        : "=r"(base_pml4)
    );
    init_task.cr3 = clone_page_tables(base_pml4);
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(init_task.cr3)
    );
    change_pml4(init_task.cr3);
    void* addr = load_elf(path, &init_task.initial_brk);
    if (!addr) {
        panic("Failed to load init binary: %s", path);
    }
    init_task.brk = init_task.initial_brk;
    alloc_region(0x10000000000, 4096 * 128, FLAGS_PRESENT | FLAGS_RW | FLAGS_USER);
    void* kstack = (char*)kmalloc(4096 * 32) + 4096 * 32;
    init_task.kernel_stack = kstack;
    set_rsp0((uint64_t)kstack);
    current_task = &init_task;
    scheduler_initialized = 1;
    jump_to_user(addr, (char*)0x10000000000 + 4096 * 128 - 16);
}

void run_next(iframe_t* iframe) {
    if (!scheduler_initialized) return;
    current_task->iframe = iframe;
    current_task->state = STATE_READY;
    do {
        current_task = current_task->next;
    } while (current_task->state != STATE_READY);
    current_task->state = STATE_RUNNING;
    ticks_remaining = current_task->time_slice;
    gc_tasks();
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(current_task->cr3)
    );
    change_pml4(current_task->cr3);
    set_rsp0((uint64_t)current_task->kernel_stack);
    current_task->iframe->rflags |= 0x200;
    context_switch(current_task->iframe);
}

void exit(int ret) {
    if (current_task == &init_task) panic("Init process exited!");
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

    // Run next task
    do {
        current_task = current_task->next;
    } while (current_task->state != STATE_READY);
    current_task->state = STATE_RUNNING;
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(current_task->cr3)
    );
    change_pml4(current_task->cr3);
    set_rsp0((uint64_t)current_task->kernel_stack);
    current_task->iframe->rflags |= 0x200;
    context_switch(current_task->iframe);
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

    // Switch to the new page table
    asm volatile("mov %0, %%cr3" :: "r"(new_task->cr3));
    change_pml4(new_task->cr3);

    // Load the ELF
    void* entry = load_elf(kpath, &new_task->initial_brk);
    if (!entry) {
        // Restore old page table
        asm volatile("mov %0, %%cr3" :: "r"(current_task->cr3));
        change_pml4(current_task->cr3);
        return -1;
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
    cleanup_memory(current_task);
    do {
        current_task = current_task->next;
    } while (current_task->state != STATE_READY);
    current_task->state = STATE_RUNNING;
    ticks_remaining = current_task->time_slice;
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(current_task->cr3)
    );
    change_pml4(current_task->cr3);
    set_rsp0((uint64_t)current_task->kernel_stack);
    current_task->iframe->rflags |= 0x200;
    context_switch(current_task->iframe);
    return 0;
}

void sleep(uint64_t ms, iframe_t *iframe) {
    if (ms == 0) return;
    current_task->state = STATE_BLOCKED;
    current_task->block_reason = BLOCK_DELAY;
    current_task->blocked_ticks = ms;
    do {
        current_task = current_task->next;
    } while (current_task->state != STATE_READY);
    current_task->state = STATE_RUNNING;
    ticks_remaining = current_task->time_slice;
    asm volatile(
        "mov %0, %%cr3"
        :: "r"(current_task->cr3)
    );
    change_pml4(current_task->cr3);
    set_rsp0((uint64_t)current_task->kernel_stack);
    current_task->iframe->rflags |= 0x200;
    context_switch(current_task->iframe);
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
        if (child == NULL) return -1;
        if (child->state == STATE_ZOMBIE) {
            if (wstatus) *wstatus = child->return_code;
            cleanup_memory(child);
            return pid;
        }
        if (options & WNOHANG) return -1;
        current_task->state = STATE_BLOCKED;
        current_task->block_reason = BLOCK_WAITPID;
        current_task->blocked_process = child;
        current_task->wstatus = wstatus;
        do {
            current_task = current_task->next;
        } while (current_task->state != STATE_READY);
        current_task->state = STATE_RUNNING;
        ticks_remaining = current_task->time_slice;
        asm volatile(
            "mov %0, %%cr3"
            :: "r"(current_task->cr3)
        );
        change_pml4(current_task->cr3);
        set_rsp0((uint64_t)current_task->kernel_stack);
        current_task->iframe->rflags |= 0x200;
        context_switch(current_task->iframe);
    } else {
        task_t* child = get_first_zombie(current_task);
        if (child != NULL) {
            if (wstatus) *wstatus = child->return_code;
            cleanup_memory(child);
            return child->pid;
        }
        if (options & WNOHANG) return -1;
        current_task->state = STATE_BLOCKED;
        current_task->block_reason = BLOCK_WAITPID;
        current_task->blocked_process = NULL;
        current_task->wstatus = wstatus;
        do {
            current_task = current_task->next;
        } while (current_task->state != STATE_READY);
        current_task->state = STATE_RUNNING;
        ticks_remaining = current_task->time_slice;
        asm volatile(
            "mov %0, %%cr3"
            :: "r"(current_task->cr3)
        );
        change_pml4(current_task->cr3);
        set_rsp0((uint64_t)current_task->kernel_stack);
        current_task->iframe->rflags |= 0x200;
        context_switch(current_task->iframe);
    }

    return -1;
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
                        cleanup_memory(task->blocked_process);
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
                    cleanup_memory(blocked);
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
