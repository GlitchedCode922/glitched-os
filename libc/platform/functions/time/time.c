/* time( time_t * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <time.h>
#include <syscall.h>

#ifndef REGTEST

/* See comments in _PDCLIB_config.h on the semantics of time_t. */

time_t time( time_t * timer ) {
    time_t t = syscall(SYSCALL_GET_TIME, 0, 0, 0, 0, 0, 0);
    if (timer) {
        *timer = t;
    }
    return t;
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

int main( void )
{
    time_t t = time( NULL );
    printf( "%d\n", (int)t );
    TESTCASE( NO_TESTDRIVER );
    return TEST_RESULTS;
}

#endif
