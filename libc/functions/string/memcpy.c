/* memcpy( void *, const void *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <stdint.h>

#ifndef REGTEST

void* memcpy(void* _PDCLIB_restrict dest, const void* _PDCLIB_restrict src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    while (n && ((uint64_t)d & 7)) {
        *d++ = *s++;
        n--;
    }

    while (n >= 8) {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8;
        s += 8;
        n -= 8;
    }

    while (n--) *d++ = *s++;
    return dest;
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

int main( void )
{
    char s[] = "xxxxxxxxxxx";
    TESTCASE( memcpy( s, abcde, 6 ) == s );
    TESTCASE( s[4] == 'e' );
    TESTCASE( s[5] == '\0' );
    TESTCASE( memcpy( s + 5, abcde, 5 ) == s + 5 );
    TESTCASE( s[9] == 'e' );
    TESTCASE( s[10] == 'x' );
    return TEST_RESULTS;
}

#endif
