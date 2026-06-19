#include "syscalls.h"
#include "fd.h"
#include "break.h"
#include "../drivers/block.h"
#include "../drivers/block/ata.h"
#include "../vfs.h"
#include "../drivers/timer.h"
#include "../console.h"
#include "../power.h"
#include "../net/udp.h"
#include "../net/icmp.h"
#include "../net/ip.h"
#include "../drivers/net.h"
#include "../panic.h"
#include "../error.h"
#include "scheduler.h"
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
        ret = create_file((const char*)arg1);
        break;
    case SYSCALL_DELETE_FILE:
        ret = remove_file((const char*)arg1);
        break;
    case SYSCALL_CREATE_DIR:
        ret = create_directory((const char*)arg1);
        break;
    case SYSCALL_GET_PPID:
        // Not implemented, will come with process management
        break;
    case SYSCALL_READDIR:
        ret = readdir((const char*)arg1, arg2, (dirent_t*)arg3);
        break;
    case SYSCALL_STAT:
        ret = stat((const char*)arg1, (stat_t*)arg2);
        break;
    case SYSCALL_FORK:
        ret = fork(iframe);
        break;
    case SYSCALL_EXECV:
        execv((char*)arg1, (char**)arg2, iframe);
        break;
    case SYSCALL_GET_TIME:
        // Not implemented, will come with RTC
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
        ret = chdir((char*)arg1);
        break;
    case SYSCALL_GETCWD:
        getcwd((char*)arg1, arg2);
        break;
    case SYSCALL_SEND_UDP:
        udp_send((uint8_t*)arg1, arg2, arg3, (uint8_t*)arg4, arg5);
        break;
    case SYSCALL_LISTEN_UDP:
        // Temporarily disabled
        //register_udp_listener(arg1, (void (*)(uint8_t*, uint16_t, uint8_t*, int))arg2);
        ret = -ENOSYS;
        break;
    case SYSCALL_STOP_UDP_LISTEN:
        //unregister_udp_listener(arg1);
        ret = -ENOSYS;
        break;
    case SYSCALL_PING:
        ping((uint8_t*)arg1);
        break;
    case SYSCALL_GET_MAC:
        ret = get_mac(arg1, (uint8_t*)arg2);
        break;
    case SYSCALL_GET_IP:
        ret = get_ip(arg1, (uint32_t*)arg2);
        break;
    case SYSCALL_ADD_ROUTE:
        add_route((uint8_t*)arg1, (uint8_t*)arg2, (uint8_t*)arg3, arg4);
        break;
    case SYSCALL_REMOVE_ROUTE:
        remove_route((uint8_t*)arg1, (uint8_t*)arg2);
        break;
    case SYSCALL_SETUP_AUTOMATIC_ROUTING:
        setup_automatic_routing();
        break;
    case SYSCALL_CONFIG_DHCP:
        ret = configure_network_interface_dhcp(arg1);
        break;
    case SYSCALL_CONFIG_STATIC:
        ret = configure_network_interface_static(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_MOUNT:
        ret = mount_filesystem((const char*)arg1, (const char*)arg2, (const char*)arg3, arg4);
        break;
    case SYSCALL_UNMOUNT:
        ret = unmount_filesystem((const char*)arg1);
        break;
    case SYSCALL_UNMOUNT_ALL:
        unmount_all_filesystems();
        ret = 0;
        break;
    case SYSCALL_OPEN_FILE:
        ret = open_file((const char*)arg1, (uint16_t)arg2);
        break;
    case SYSCALL_OPEN_CONSOLE:
        ret = open_console((uint16_t)arg1);
        break;
    case SYSCALL_OPEN_FRAMEBUFFER:
        ret = open_framebuffer((uint16_t)arg1);
        break;
    case SYSCALL_CLOSE:
        ret = close((int)arg1);
        break;
    case SYSCALL_READ:
        ret = read((int)arg1, (void*)arg2, (size_t)arg3);
        break;
    case SYSCALL_WRITE:
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
    case SYSCALL_OPEN_SERIAL:
        ret = open_serial(arg1, arg2);
        break;
    case SYSCALL_YIELD:
        run_next(iframe);
        break;
    case SYSCALL_WAITPID:
        ret = waitpid(arg1, (int*)arg2, arg3, iframe);
        break;
    case SYSCALL_SPAWN:
        ret = spawn((char*)arg1, (char**)arg2, iframe);
        break;
    case SYSCALL_ISATTY:
        ret = isatty(arg1);
        break;
    case SYSCALL_TCGETATTR:
        ret = tcgetattr(arg1, (termios_t*)arg2);
        break;
    case SYSCALL_TCSETATTR:
        ret = tcsetattr(arg1, (termios_t*)arg2);
        break;
    case SYSCALL_DRIVE_LOAD_EJECT:
        ret = ata_load_eject(arg1, arg2);
        break;
    case SYSCALL_SETFONT:
        setfont((font_t*)arg1);
        break;
    case SYSCALL_RENAME_FILE:
        ret = rename_file((const char*)arg1, (const char*)arg2);
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
