/* memset( void *, int, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <stdint.h>

#ifndef REGTEST

void* memset(void* ptr, int value, size_t n) {
    unsigned char *p = (unsigned char *)ptr;
    unsigned char c = (unsigned char)value;

    while (n && ((uintptr_t)p & sizeof((uintptr_t) - 1))) {
        *p++ = c;
        n--;
    }

    uint64_t word = 0;
    for (size_t i = 0; i < 8; i++) {
        word = (word << 8) | c;
    }

    uint64_t* w = (uint64_t*)p;
    while (n >= 8) {
        *w++ = word;
        n -= 8;
    }

    p = (unsigned char*)w;
    while (n--) *p++ = c;

    return ptr;
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

int main( void )
{
    char s[] = "xxxxxxxxx";
    TESTCASE( memset( s, 'o', 10 ) == s );
    TESTCASE( s[9] == 'o' );
    TESTCASE( memset( s, '_', ( 0 ) ) == s );
    TESTCASE( s[0] == 'o' );
    TESTCASE( memset( s, '_', 1 ) == s );
    TESTCASE( s[0] == '_' );
    TESTCASE( s[1] == 'o' );
    return TEST_RESULTS;
}

#endif
