#pragma once
#include <stdint.h>

typedef uint64_t dev_t;

#define POSIX_COMPAT_UNION(type, name1, name2) \
    union { \
        type name1; \
        type name2; \
    }

typedef struct stat {
    POSIX_COMPAT_UNION(uint64_t, size, st_size);
    POSIX_COMPAT_UNION(uint64_t, ctime, st_ctime);
    POSIX_COMPAT_UNION(uint64_t, mtime, st_mtime);
    POSIX_COMPAT_UNION(uint64_t, btime, st_btime);
    POSIX_COMPAT_UNION(dev_t, rdev, st_rdev);
    POSIX_COMPAT_UNION(uint32_t, mode, st_mode);
    uint32_t type;
} __attribute__((packed)) stat_t;

#define S_IFMT 0170000
#define S_IFSOCK 0140000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFBLK 0060000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFIFO 0010000

#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
#define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)

#undef POSIX_COMPAT_UNION
