#include "syscalls.h"
#include "../drivers/block.h"
#include "../mount.h"
#include "../drivers/timer.h"
#include "../memory/paging.h"
#include "../drivers/kbd.h"
#include "../console.h"
#include "../power.h"
#include "../net/udp.h"
#include "../net/icmp.h"
#include "../net/ip.h"
#include "../drivers/net.h"
#include "exec.h"
#include <stdarg.h>
#include <stdint.h>

uint64_t syscall(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    asm volatile("sti");
    switch (syscall_number) {
    case SYSCALL_EXIT:
        // Not implemented, will come with process management
        return 0;
    case SYSCALL_READ_FILE:
        return read_file((const char*)arg1, (uint8_t*)arg2, arg3, arg4);
    case SYSCALL_WRITE_FILE:
        return write_file((const char*)arg1, (const uint8_t*)arg2, arg3, arg4);
    case SYSCALL_CREATE_FILE:
        return create_file((const char*)arg1);
    case SYSCALL_DELETE_FILE:
        return remove_file((const char*)arg1);
    case SYSCALL_CREATE_DIR:
        return create_directory((const char*)arg1);
    case SYSCALL_GET_PPID:
        // Not implemented, will come with process management
        return 0;
    case SYSCALL_READ_SECTORS:
        return read_sectors(arg1, arg2, (uint8_t*)arg3, arg4);
    case SYSCALL_WRITE_SECTORS:
        return write_sectors(arg1, arg2, (uint8_t*)arg3, arg4);
    case SYSCALL_LIST_DIR:
        return list_directory((const char*)arg1, (char*)arg2, arg3);
    case SYSCALL_GET_FILE_SIZE:
        return get_file_size((const char*)arg1);
    case SYSCALL_FORK:
        // Not implemented, will come with process management
        return 0;
    case SYSCALL_EXECUTE:
        return execve((const char*)arg1, (const char**)arg2, (const char**)arg3);
    case SYSCALL_GET_TIME:
        // Not implemented, will come with RTC
        return 0;
    case SYSCALL_GET_PID:
        // Not implemented, will come with process management
        return 0;
    case SYSCALL_GET_UPTIME:
        return get_uptime_milliseconds();
    case SYSCALL_DELAY: {
        // When process management is implemented, this will yield the process
        uint64_t start_time = get_uptime_milliseconds();
        while (get_uptime_milliseconds() - start_time < arg1) {
            // Busy wait
        }
        return 0;
    }
    case SYSCALL_READ_CONSOLE:
        return (uintptr_t)kbdinput();
    case SYSCALL_WRITE_CONSOLE:
        kprintf((const char*)arg1, arg2, arg3, arg4, arg5);
        return 0;
    case SYSCALL_ALLOC_PAGE:
        return (uintptr_t)alloc_page(arg1, arg2);
    case SYSCALL_FREE_PAGE:
        free_page((void*)arg1);
        return 0;
    case SYSCALL_GETENV:
        return (uintptr_t)getenv((const char *)arg1);
    case SYSCALL_REBOOT:
        // Reboot the system
        asm volatile("cli"); // Disable interrupts
        reboot();
        return 0; // This line will not be reached
    case SYSCALL_VPRINTF:
        kvprintf((const char*)arg1, *(va_list*)arg2);
        return 0;
    case SYSCALL_CHDIR:
        return chdir((char*)arg1);
    case SYSCALL_GETCWD:
        getcwd((char*)arg1);
        return 0;
    case SYSCALL_FILE_EXISTS:
        return exists((const char*)arg1);
    case SYSCALL_IS_DIRECTORY:
        return is_directory((const char*)arg1);
    case SYSCALL_SEND_UDP:
        udp_send((uint8_t*)arg1, arg2, arg3, (uint8_t*)arg4, arg5);
        return 0;
    case SYSCALL_LISTEN_UDP:
        register_udp_listener(arg1, (void (*)(uint8_t*, uint16_t, uint8_t*, int))arg2);
        return 0;
    case SYSCALL_STOP_UDP_LISTEN:
        unregister_udp_listener(arg1);
        return 0;
    case SYSCALL_PING:
        ping((uint8_t*)arg1);
        return 0;
    case SYSCALL_GET_MAC:
        return get_mac(arg1, (uint8_t*)arg2);
    case SYSCALL_GET_IP:
        return get_ip(arg1, (uint32_t*)arg2);
    case SYSCALL_ADD_ROUTE:
        add_route((uint8_t*)arg1, (uint8_t*)arg2, (uint8_t*)arg3, arg4);
        return 0;
    case SYSCALL_REMOVE_ROUTE:
        remove_route((uint8_t*)arg1, (uint8_t*)arg2);
        return 0;
    case SYSCALL_SETUP_AUTOMATIC_ROUTING:
        setup_automatic_routing();
        return 0;
    case SYSCALL_CONFIG_DHCP:
        return configure_network_interface_dhcp(arg1);
    case SYSCALL_CONFIG_STATIC:
        return configure_network_interface_static(arg1, arg2, arg3, arg4);
    case SYSCALL_MOUNT:
        return mount_filesystem((const char*)arg1, (const char*)arg2, arg3, arg4, arg5);
    case SYSCALL_UNMOUNT:
        return unmount_filesystem((const char*)arg1);
    case SYSCALL_UNMOUNT_ALL:
        unmount_all_filesystems();
        return 0;
    default:
        // Invalid syscall, return an error code
        return 0xFFFFFFFFFFFFFFFF;
    }
}
