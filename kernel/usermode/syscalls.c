#include "usermode/syscalls.h"
#include "usermode/fd.h"
#include "usermode/break.h"
#include "usermode/scheduler.h"
#include "net/icmp.h"
#include "net/ip.h"
#include "drivers/timer.h"
#include "memory/paging.h"
#include "vfs.h"
#include "console.h"
#include "power.h"
#include "panic.h"
#include "error.h"
#include <stdarg.h>
#include <stdint.h>

extern void syscall_entry();

static uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile(
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile(
        "wrmsr"
        :: "c"(msr), "a"(low), "d"(high)
    );
}

void syscall_init() {
    wrmsr(0xC0000081, ((uint64_t)KERNEL_CS << 32) | ((uint64_t)USER_CS << 48));
    wrmsr(0xC0000084, 0x200);
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    uint64_t ia32_efer = rdmsr(0xC0000080);
    ia32_efer |= 1;
    wrmsr(0xC0000080, ia32_efer);
}

void syscall(iframe_t* iframe) {
    uint64_t syscall_number = iframe->rax;
    uint64_t arg1 = iframe->rdi;
    uint64_t arg2 = iframe->rsi;
    uint64_t arg3 = iframe->rdx;
    uint64_t arg4 = iframe->r10;
    uint64_t arg5 = iframe->r8;
    uint64_t arg6 = iframe->r9;
    int64_t ret = 0;
    asm volatile("sti");
    switch (syscall_number) {
    case SYSCALL_EXIT:
        exit((int)arg1);
        break;
    case SYSCALL_CREATE_FILE:
        ret = validate_user_string((const char*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = create_file((const char*)arg1);
        break;
    case SYSCALL_DELETE_FILE:
        ret = validate_user_string((const char*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = remove_file((const char*)arg1);
        break;
    case SYSCALL_CREATE_DIR:
        ret = validate_user_string((const char*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = create_directory((const char*)arg1);
        break;
    case SYSCALL_GET_PPID:
        ret = getppid();
        break;
    case SYSCALL_READDIR:
        ret = validate_user_pointer((void*)arg2, sizeof(dirent_t), 1);
        if (ret < 0) break;
        ret = fd_readdir(arg1, (dirent_t*)arg2);
        break;
    case SYSCALL_STAT:
        ret = validate_user_string((const char*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = validate_user_pointer((void*)arg2, sizeof(stat_t), 1);
        if (ret < 0) break;
        ret = stat((const char*)arg1, (stat_t*)arg2);
        break;
    case SYSCALL_FORK:
        ret = fork(iframe);
        break;
    case SYSCALL_EXECVE:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        execve((char*)arg1, (char**)arg2, (char**)arg3, iframe);
        break;
    case SYSCALL_GET_TIME:
        ret = get_time();
        break;
    case SYSCALL_GETPID:
        ret = getpid();
        break;
    case SYSCALL_GET_UPTIME:
        ret = get_uptime_milliseconds();
        break;
    case SYSCALL_SLEEP:
        sleep(arg1, iframe);
        break;
    case SYSCALL_BRK:
        ret = (uintptr_t)set_brk((void*)arg1);
        break;
    case SYSCALL_SBRK:
        ret = (uintptr_t)sbrk((intptr_t)arg1);
        break;
    case SYSCALL_REBOOT:
        // Reboot the system
        asm volatile("cli"); // Disable interrupts
        reboot();
        panic("reboot");
        break; // This line will not be reached
    case SYSCALL_CHDIR:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = chdir((char*)arg1);
        break;
    case SYSCALL_GETCWD:
        ret = validate_user_pointer((void*)arg1, arg2, 1);
        if (ret < 0) break;
        getcwd((char*)arg1, arg2);
        break;
    case SYSCALL_PING:
        ret = validate_user_pointer((void*)arg1, 4, 0);
        if (ret < 0) break;
        ret = ping((uint8_t*)arg1, arg2);
        break;
    case SYSCALL_ADD_ROUTE:
        ret = validate_user_pointer((void*)arg1, 4, 0);
        if (ret < 0) break;
        ret = validate_user_pointer((void*)arg2, 4, 0);
        if (ret < 0) break;
        ret = validate_user_pointer((void*)arg3, 4, 0);
        if (ret < 0) break;
        ret = validate_user_string((void*)arg4, MAX_PATH);
        if (ret < 0) break;
        add_route((uint8_t*)arg1, (uint8_t*)arg2, (uint8_t*)arg3, (char*)arg4);
        break;
    case SYSCALL_REMOVE_ROUTE:
        ret = validate_user_pointer((void*)arg1, 4, 0);
        if (ret < 0) break;
        ret = validate_user_pointer((void*)arg2, 4, 0);
        if (ret < 0) break;
        remove_route((uint8_t*)arg1, (uint8_t*)arg2);
        break;
    case SYSCALL_MOUNT:
        if (arg1) {
            ret = validate_user_string((void*)arg1, MAX_PATH);
            if (ret < 0) break;
        }
        ret = validate_user_string((void*)arg2, MAX_PATH);
        if (ret < 0) break;
        ret = validate_user_string((void*)arg3, 32);
        if (ret < 0) break;
        ret = mount_filesystem((const char*)arg1, (const char*)arg2, (const char*)arg3, arg4);
        break;
    case SYSCALL_UNMOUNT:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = unmount_filesystem((const char*)arg1);
        break;
    case SYSCALL_OPEN:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = fd_open((const char*)arg1, (uint16_t)arg2);
        break;
    case SYSCALL_CLOSE:
        ret = fd_close((int)arg1);
        break;
    case SYSCALL_READ:
        ret = validate_user_pointer((void*)arg2, arg3, 1);
        if (ret < 0) break;
        ret = read((int)arg1, (void*)arg2, (size_t)arg3);
        break;
    case SYSCALL_WRITE:
        ret = validate_user_pointer((void*)arg2, arg3, 0);
        if (ret < 0) break;
        ret = write((int)arg1, (const void*)arg2, (size_t)arg3);
        break;
    case SYSCALL_SEEK:
        ret = seek((int)arg1, (size_t)arg2, (int)arg3);
        break;
    case SYSCALL_DUP:
        ret = dup((int)arg1);
        break;
    case SYSCALL_DUP2:
        ret = dup2((int)arg1, (int)arg2);
        break;
    case SYSCALL_YIELD:
        run_next(iframe);
        break;
    case SYSCALL_WAITPID:
        if (arg2) {
            ret = validate_user_pointer((void*)arg2, sizeof(int), 1);
            if (ret < 0) break;
        }
        ret = waitpid(arg1, (int*)arg2, arg3, iframe);
        break;
    case SYSCALL_SPAWN:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = spawn((char*)arg1, (char**)arg2, (char**)arg3, iframe);
        break;
    case SYSCALL_IOCTL:
        ret = fd_ioctl(arg1, arg2, arg3);
        break;
    case SYSCALL_SETFONT:
        ret = validate_user_pointer((void*)arg1, sizeof(font_t), 0);
        if (ret < 0) break;
        setfont((font_t*)arg1);
        break;
    case SYSCALL_RENAME_FILE:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = validate_user_string((void*)arg2, MAX_PATH);
        if (ret < 0) break;
        ret = rename_file((const char*)arg1, (const char*)arg2);
        break;
    case SYSCALL_MKNOD:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = mknod((const char*)arg1, arg2, arg3);
        break;
    case SYSCALL_TELL:
        ret = tell(arg1);
        break;
    case SYSCALL_LINK:
        ret = validate_user_string((void*)arg1, MAX_PATH);
        if (ret < 0) break;
        ret = validate_user_string((void*)arg2, MAX_PATH);
        if (ret < 0) break;
        ret = link((const char*)arg1, (const char*)arg2);
        break;
    case SYSCALL_SOCKET:
        ret = fd_socket(arg1, arg2, arg3);
        break;
    case SYSCALL_BIND:
        ret = validate_user_pointer((void*)arg2, sizeof(sockaddr_in_t), 0);
        if (ret < 0) break;
        ret = fd_bind(arg1, (sockaddr_in_t*)arg2);
        break;
    case SYSCALL_UNBIND:
        ret = fd_unbind(arg1);
        break;
    case SYSCALL_RECVFROM:
        ret = validate_user_pointer((void*)arg2, arg3, 1);
        if (ret < 0) break;
        ret = validate_user_pointer((void*)arg5, sizeof(sockaddr_in_t), 1);
        if (ret < 0) break;
        ret = fd_recvfrom(arg1, (uint8_t*)arg2, arg3, arg4, (sockaddr_in_t*)arg5);
        break;
    case SYSCALL_SENDTO:
        ret = validate_user_pointer((void*)arg2, arg3, 0);
        if (ret < 0) break;
        ret = validate_user_pointer((void*)arg5, sizeof(sockaddr_in_t), 0);
        if (ret < 0) break;
        ret = fd_sendto(arg1, (const uint8_t*)arg2, arg3, arg4, (const sockaddr_in_t*)arg5);
        break;
    case SYSCALL_FSTAT:
        ret = validate_user_pointer((void*)arg2, sizeof(stat_t), 1);
        if (ret < 0) break;
        ret = fstat(arg1, (stat_t*)arg2);
        break;
    case SYSCALL_TRUNCATE:
        ret = validate_user_string((char*)arg1, MAX_PATH);
        if (ret < 0) break;
        file_handle_t handle;
        ret = lookup((char*)arg1, &handle);
        if (ret < 0) break;
        ret = truncate(handle, arg2);
        break;
    case SYSCALL_FTRUNCATE:
        ret = ftruncate(arg1, arg2);
        break;
    default:
        // Invalid syscall, return an error code
        ret = -ENOSYS;
    }
    iframe->rax = (uint64_t)ret;
    if (ticks_remaining <= 0 && iframe->cs == USER_CS) {
        run_next(iframe); // Next task
    }
}
