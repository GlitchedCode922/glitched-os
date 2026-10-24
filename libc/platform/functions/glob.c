#include <glob.h>
#include <fnmatch.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    glob_t *out;
    int flags;
    int (*errfunc)(const char *, int);
    int error;
} glob_ctx;

static int has_magic(const char *s) {
    while (*s) {
        if (*s == '\\' && s[1]) {
            s += 2;
            continue;
        }
        if (*s == '*' || *s == '?' || *s == '[') return 1;
        s++;
    }

    return 0;
}

static int add_string(glob_t *out, const char *path) {
    size_t offset = out->gl_offs;
    size_t count = out->gl_pathc;

    char *value = strdup(path);
    if (!value) return GLOB_NOSPACE;

    char **newv = realloc(out->gl_pathv, (offset + count + 2) * sizeof(char *));
    if (!newv) {
        free(value);
        return GLOB_NOSPACE;
    }

    out->gl_pathv = newv;
    out->gl_pathv[offset + count] = value;
    out->gl_pathv[offset + count + 1] = NULL;
    out->gl_pathc++;

    return 0;
}

static char *join_path(const char *prefix, const char *name) {
    size_t a = strlen(prefix);
    size_t b = strlen(name);
    int slash = a != 0 && prefix[a - 1] != '/';

    char *result = malloc(a + slash + b + 1);
    if (!result) return NULL;

    memcpy(result, prefix, a);

    if (slash) result[a++] = '/';

    memcpy(result + a, name, b + 1);
    return result;
}

static int add_result(glob_ctx *ctx, const char *path) {
    struct stat st;
    char *value;
    char **newv;
    size_t offset = ctx->out->gl_offs;
    size_t count = ctx->out->gl_pathc;

    if (stat(path, &st) < 0)
        return 0;

    value = strdup(path);
    if (!value) {
        ctx->error = GLOB_NOSPACE;
        return -1;
    }

    if ((ctx->flags & GLOB_MARK) && S_ISDIR(st.st_mode)) {
        size_t n = strlen(value);

        if (n == 0 || value[n - 1] != '/') {
            char *marked = realloc(value, n + 2);

            if (!marked) {
                free(value);
                ctx->error = GLOB_NOSPACE;
                return -1;
            }

            value = marked;
            value[n] = '/';
            value[n + 1] = '\0';
        }
    }

    newv = realloc(ctx->out->gl_pathv, (offset + count + 2) * sizeof(char *));

    if (!newv) {
        free(value);
        ctx->error = GLOB_NOSPACE;
        return -1;
    }

    ctx->out->gl_pathv = newv;
    ctx->out->gl_pathv[offset + count] = value;
    ctx->out->gl_pathv[offset + count + 1] = NULL;
    ctx->out->gl_pathc++;

    return 0;
}

static int compare_paths(const void *a, const void *b) {
    const char *const *pa = a;
    const char *const *pb = b;

    return strcmp(*pa, *pb);
}

static int report_error(glob_ctx *ctx, const char *path) {
    int err = errno;

    if (ctx->errfunc && ctx->errfunc(path, err)) return 1;

    return (ctx->flags & GLOB_ERR) != 0;
}

static void walk(glob_ctx *ctx, const char *pattern, const char *prefix) {
    const char *slash;
    char *component;
    size_t len;
    int more;
    int magic;

    if (ctx->error)  return;

    /* An empty remainder means the prefix itself is the result. */
    if (*pattern == '\0') {
        add_result(ctx, *prefix ? prefix : ".");
        return;
    }

    slash = strchr(pattern, '/');
    more = slash != NULL;
    len = more ? (size_t)(slash - pattern) : strlen(pattern);

    component = malloc(len + 1);
    if (!component) {
        ctx->error = GLOB_NOSPACE;
        return;
    }

    memcpy(component, pattern, len);
    component[len] = '\0';

    /* Skip repeated separators. */
    while (more && slash[1] == '/') slash++;

    magic = has_magic(component);

    if (!magic) {
        char *candidate = join_path(prefix, component);
        struct stat st;

        if (!candidate) {
            ctx->error = GLOB_NOSPACE;
            free(component);
            return;
        }

        if (more) {
            if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
                walk(ctx, slash + 1, candidate);
            }
        } else {
            add_result(ctx, candidate);
        }

        free(candidate);
    } else {
        const char *dirpath = *prefix ? prefix : ".";
        DIR *dir = opendir(dirpath);

        if (!dir) {
            int saved_errno = errno;
            if (report_error(ctx, dirpath)) ctx->error = GLOB_ABORTED;
            errno = saved_errno;
        } else {
            struct dirent *entry;

            while (!ctx->error && (entry = readdir(dir)) != NULL) {
                char *candidate;
                struct stat st;
                int fnflags = 0;

                if (!(ctx->flags & GLOB_PERIOD))
                    fnflags |= FNM_PERIOD;

                if (ctx->flags & GLOB_NOESCAPE)
                    fnflags |= FNM_NOESCAPE;

                if (fnmatch(component, entry->d_name, fnflags) != 0)
                    continue;

                /* Do not traverse "." or ".." as wildcard matches. */
                if (more &&
                    (strcmp(entry->d_name, ".") == 0 ||
                     strcmp(entry->d_name, "..") == 0))
                    continue;

                candidate = join_path(prefix, entry->d_name);
                if (!candidate) {
                    ctx->error = GLOB_NOSPACE;
                    break;
                }

                if (more) {
                    if (stat(candidate, &st) == 0 &&
                        S_ISDIR(st.st_mode)) {
                        walk(ctx, slash + 1, candidate);
                    }
                } else {
                    add_result(ctx, candidate);
                }

                free(candidate);
            }

            closedir(dir);
        }
    }

    free(component);
}

int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob) {
    glob_ctx ctx;
    size_t old_count = 0;
    size_t offset;

    if (!pattern || !pglob) return GLOB_ABORTED;

    if (flags & GLOB_APPEND) {
        old_count = pglob->gl_pathc;

        if (old_count && !pglob->gl_pathv) return GLOB_ABORTED;
    } else {
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
    }

    offset = (flags & GLOB_DOOFFS) ? pglob->gl_offs : 0;

    if (!(flags & GLOB_APPEND)) {
        pglob->gl_offs = offset;

        if (offset) {
            pglob->gl_pathv = calloc(offset + 1, sizeof(char *));
            if (!pglob->gl_pathv) return GLOB_NOSPACE;
        }
    }

    pglob->gl_flags = flags;

    ctx.out = pglob;
    ctx.flags = flags;
    ctx.errfunc = errfunc;
    ctx.error = 0;

    if (pattern[0] == '/') {
        walk(&ctx, pattern + 1, "/");
    } else {
        walk(&ctx, pattern, "");
    }

    if (ctx.error) {
        int result = ctx.error;
        globfree(pglob);
        return result;
    }

    if (pglob->gl_pathc == old_count) {
        if (flags & GLOB_NOCHECK) {
            int result = add_string(pglob, pattern);

            if (result != 0) {
                globfree(pglob);
                return result;
            }
        } else {
            if (!(flags & GLOB_APPEND)) globfree(pglob);

            return GLOB_NOMATCH;
        }
    }

    if (!(flags & GLOB_NOSORT) && pglob->gl_pathc > 1) {
        qsort(pglob->gl_pathv + pglob->gl_offs,
              pglob->gl_pathc,
              sizeof(char *), compare_paths);
    }

    return 0;
}

void globfree(glob_t *pglob){
    size_t i;

    if (!pglob)
        return;

    for (i = 0; i < pglob->gl_pathc; i++) free(pglob->gl_pathv[pglob->gl_offs + i]);

    free(pglob->gl_pathv);

    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
    pglob->gl_offs = 0;
}
