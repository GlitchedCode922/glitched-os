/* _PDCLIB_rename( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* This is an example implementation of _PDCLIB_rename() fit for use with
   POSIX kernels.
 */

#include <stdint.h>
#ifndef REGTEST

#include "pdclib/_PDCLIB_glue.h"

#include <syscall.h>

int _PDCLIB_rename( const char * oldpath, const char * newpath )
{
    /* Whether existing newpath is overwritten is implementation-
       defined. This system call *does* overwrite.
    */
    int res = syscall(SYSCALL_RENAME_FILE, (uint64_t)oldpath, (uint64_t)newpath, 0, 0, 0, 0);
    if (res < 0) {
        /* The 1:1 mapping in _PDCLIB_config.h ensures this works. */
        *_PDCLIB_errno_func() = -res;
        return -1;
    } else {
        return 0;
    }
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

#include <stdlib.h>

int main( void )
{
#ifndef REGTEST
    FILE * file;
    remove( testfile1 );
    remove( testfile2 );
    /* check that neither file exists */
    TESTCASE( fopen( testfile1, "r" ) == NULL );
    TESTCASE( fopen( testfile2, "r" ) == NULL );
    /* rename file 1 to file 2 - expected to fail */
    TESTCASE( _PDCLIB_rename( testfile1, testfile2 ) == -1 );
    /* create file 1 */
    TESTCASE( ( file = fopen( testfile1, "w" ) ) != NULL );
    TESTCASE( fputc( 'x', file ) == 'x' );
    TESTCASE( fclose( file ) == 0 );
    /* check that file 1 exists */
    TESTCASE( ( file = fopen( testfile1, "r" ) ) != NULL );
    TESTCASE( fclose( file ) == 0 );
    /* rename file 1 to file 2 */
    TESTCASE( _PDCLIB_rename( testfile1, testfile2 ) == 0 );
    /* check that file 2 exists, file 1 does not */
    TESTCASE( fopen( testfile1, "r" ) == NULL );
    TESTCASE( ( file = fopen( testfile2, "r" ) ) != NULL );
    TESTCASE( fclose( file ) == 0 );
    /* create another file 1 */
    TESTCASE( ( file = fopen( testfile1, "w" ) ) != NULL );
    TESTCASE( fputc( 'x', file ) == 'x' );
    TESTCASE( fclose( file ) == 0 );
    /* check that file 1 exists */
    TESTCASE( ( file = fopen( testfile1, "r" ) ) != NULL );
    TESTCASE( fclose( file ) == 0 );
    /* Whether existing destination files are overwritten or not
       is implementation-defined.
       This implementation *does* overwrite.
    */
    TESTCASE( _PDCLIB_rename( testfile1, testfile2 ) == 0 );
    /* remove both files */
    remove( testfile1 );
    remove( testfile2 );
#endif
    return TEST_RESULTS;
}

#endif
