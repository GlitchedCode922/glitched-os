#pragma once

#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
#define O_CREAT 0x04
#define O_NONBLOCK 0x08
#define O_DIRECTORY 0x10
#define O_APPEND 0x20
#define O_CLOEXEC 0x40
#define O_EXCL 0x80
#define O_TRUNC 0x100
#define O_ACCESS 0x03

#define F_GETFD         0
#define F_SETFD         1
#define F_GETFL         2
#define F_SETFL         3
#define F_DUPFD         4
#define F_DUPFD_CLOEXEC 5
#define FD_CLOEXEC O_CLOEXEC
