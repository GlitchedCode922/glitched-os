#include "syscalls.h"
#include "fd.h"
#include "break.h"
#include "../drivers/block.h"
#include "../mount.h"
#include "../drivers/timer.h"
#include "../memory/paging.h"
#include "../console.h"
#include "../power.h"
#include "../net/udp.h"
#include "../net/icmp.h"
#include "../net/ip.h"
#include "../drivers/net.h"
#include "../drivers/serial.h"
#include "scheduler.h"
#include <stdarg.h>
#include <stdint.h>

uint64_t syscall(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, iframe_t* iframe) {
    asm volatile("sti");
    uint64_t ret = 0;
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
        ret = 0;
        break;
    case SYSCALL_LIST_DIR:
        ret = list_directory((const char*)arg1, (char*)arg2, arg3);
        break;
    case SYSCALL_GET_FILE_SIZE:
        ret = get_file_size((const char*)arg1);
        break;
    case SYSCALL_FORK:
        ret = fork(iframe);
        break;
    case SYSCALL_EXECV:
        ret = 0;
        execv((char*)arg1, (char**)arg2, iframe);
        break;
    case SYSCALL_GET_TIME:
        // Not implemented, will come with RTC
        ret = 0;
        break;
    case SYSCALL_GETPID:
        ret = getpid();
        break;
    case SYSCALL_GET_UPTIME:
        ret = get_uptime_milliseconds();
        break;
    case SYSCALL_SLEEP:
        sleep(arg1, iframe);
        ret = 0;
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
        ret = 0; // This line will not be reached
        break;
    case SYSCALL_CHDIR:
        ret = chdir((char*)arg1);
        break;
    case SYSCALL_GETCWD:
        getcwd((char*)arg1, arg2);
        ret = 0;
        break;
    case SYSCALL_FILE_EXISTS:
        ret = exists((const char*)arg1);
        break;
    case SYSCALL_IS_DIRECTORY:
        ret = is_directory((const char*)arg1);
        break;
    case SYSCALL_SEND_UDP:
        udp_send((uint8_t*)arg1, arg2, arg3, (uint8_t*)arg4, arg5);
        ret = 0;
        break;
    case SYSCALL_LISTEN_UDP:
        // Temporarily disabled
        //register_udp_listener(arg1, (void (*)(uint8_t*, uint16_t, uint8_t*, int))arg2);
        ret = 0;
        break;
    case SYSCALL_STOP_UDP_LISTEN:
        //unregister_udp_listener(arg1);
        ret = 0;
        break;
    case SYSCALL_PING:
        ping((uint8_t*)arg1);
        ret = 0;
        break;
    case SYSCALL_GET_MAC:
        ret = get_mac(arg1, (uint8_t*)arg2);
        break;
    case SYSCALL_GET_IP:
        ret = get_ip(arg1, (uint32_t*)arg2);
        break;
    case SYSCALL_ADD_ROUTE:
        add_route((uint8_t*)arg1, (uint8_t*)arg2, (uint8_t*)arg3, arg4);
        ret = 0;
        break;
    case SYSCALL_REMOVE_ROUTE:
        remove_route((uint8_t*)arg1, (uint8_t*)arg2);
        ret = 0;
        break;
    case SYSCALL_SETUP_AUTOMATIC_ROUTING:
        setup_automatic_routing();
        ret = 0;
        break;
    case SYSCALL_CONFIG_DHCP:
        ret = configure_network_interface_dhcp(arg1);
        break;
    case SYSCALL_CONFIG_STATIC:
        ret = configure_network_interface_static(arg1, arg2, arg3, arg4);
        break;
    case SYSCALL_MOUNT:
        ret = mount_filesystem((const char*)arg1, (const char*)arg2, arg3, arg4, arg5);
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
    case SYSCALL_GETPPID:
        ret = getppid();
        break;
    default:
        // Invalid syscall, return an error code
        ret = 0xFFFFFFFFFFFFFFFF;
    }
    iframe->rax = ret;
    return ret;
}
