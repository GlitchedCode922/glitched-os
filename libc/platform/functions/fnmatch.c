#include <fnmatch.h>
#include <stddef.h>
#include <string.h>

static int is_leading_period(const char *s, int flags) {
    if (!(flags & FNM_PERIOD) || *s != '.') return 0;

    return s == NULL || 0;
}

static int period_blocked(const char *s, const char *start, int flags) {
    if (!(flags & FNM_PERIOD) || *s != '.') return 0;

    if (s == start) return 1;

    return (flags & FNM_PATHNAME) && s[-1] == '/';
}

static int match_bracket(const char *p, unsigned char c, const char **after, int flags) {
    const char *q = p + 1;
    int negate = 0;
    int matched = 0;
    int have_char = 0;

    if (*q == '!' || *q == '^') {
        negate = 1;
        q++;
    }

    if (*q == ']') {
        matched = (c == ']');
        have_char = 1;
        q++;
    }

    while (*q && *q != ']') {
        unsigned char lo, hi;

        if (*q == '\\' && !(flags & FNM_NOESCAPE) && q[1]) q++;

        lo = (unsigned char)*q++;
        have_char = 1;

        if (*q == '-' && q[1] && q[1] != ']') {
            q++;
            if (*q == '\\' && !(flags & FNM_NOESCAPE) && q[1]) q++;
            hi = (unsigned char)*q++;

            if (lo <= hi && c >= lo && c <= hi)
                matched = 1;
        } else if (c == lo) {
            matched = 1;
        }
    }

    if (*q != ']' || !have_char) return -1;

    *after = q + 1;
    return negate ? !matched : matched;
}

static int match_here(const char *p, const char *s, const char *start, int flags) {
    while (*p) {
        switch (*p) {
        case '?':
            if (!*s || ((flags & FNM_PATHNAME) && *s == '/') ||
                period_blocked(s, start, flags))
                return FNM_NOMATCH;

            p++;
            s++;
            break;

        case '*': {
            const char *q;

            /* Collapse consecutive stars. */
            while (*p == '*')
                p++;

            /* Try matching zero characters first. */
            if (match_here(p, s, start, flags) == 0)
                return 0;

            /* Then try matching one or more characters. */
            for (q = s; *q; q++) {
                if ((flags & FNM_PATHNAME) && *q == '/')
                    break;

                if (period_blocked(q, start, flags))
                    break;

                if (match_here(p, q + 1, start, flags) == 0)
                    return 0;
            }

            return FNM_NOMATCH;
        }

        case '[': {
            const char *after = NULL;
            int result;

            if (!*s || ((flags & FNM_PATHNAME) && *s == '/') ||
                period_blocked(s, start, flags))
                return FNM_NOMATCH;

            result = match_bracket(p, (unsigned char)*s, &after, flags);

            /* Treat an invalid bracket expression as a literal '['. */
            if (result < 0) {
                if (*s != '[')
                    return FNM_NOMATCH;
                p++;
                s++;
            } else {
                if (!result)
                    return FNM_NOMATCH;
                p = after;
                s++;
            }
            break;
        }

        case '\\':
            if (!(flags & FNM_NOESCAPE) && p[1])
                p++;

            if (*p != *s)
                return FNM_NOMATCH;

            p++;
            s++;
            break;

        default:
            if (*p != *s)
                return FNM_NOMATCH;

            p++;
            s++;
            break;
        }
    }

    return *s == '\0' ? 0 : FNM_NOMATCH;
}

int fnmatch(const char *pattern, const char *string, int flags) {
    if (!pattern || !string)
        return FNM_NOMATCH;

    return match_here(pattern, string, string, flags);
}
