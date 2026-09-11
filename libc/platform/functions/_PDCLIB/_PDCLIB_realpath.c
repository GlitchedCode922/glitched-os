/* _PDCLIB_realpath( const char * path )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef REGTEST

#include "pdclib/_PDCLIB_glue.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define PATH_MAX 4096

char* realpath(const char* path, char* resolved_path) {
    char cwd[PATH_MAX];
    char* work;
    char* component;
    size_t len;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (*path == '\0') {
        errno = ENOENT;
        return NULL;
    }

    if (path[0] == '/') {
        len = strlen(path) + 1;
        work = malloc(len);
        if (work == NULL) {
            return NULL;
        }

        strcpy(work, path);
    } else {
        getcwd(cwd, sizeof(cwd));

        len = strlen(cwd) + 1 + strlen(path) + 1;
        work = malloc(len);
        if (work == NULL) {
            return NULL;
        }

        snprintf(work, len, "%s/%s", cwd, path);
    }

    if (resolved_path == NULL) {
        resolved_path = malloc(PATH_MAX);
        if (resolved_path == NULL) {
            free(work);
            return NULL;
        }
    }

    resolved_path[0] = '/';
    resolved_path[1] = '\0';

    component = strtok(work, "/");

    while (component != NULL) {
        if (strcmp(component, ".") == 0) {
            /* Ignore "." */
        } else if (strcmp(component, "..") == 0) {
            size_t out_len = strlen(resolved_path);

            if (out_len > 1) {
                char* slash = strrchr(resolved_path, '/');

                if (slash != NULL) {
                    *slash = '\0';
                }
            }
        } else {
            size_t out_len = strlen(resolved_path);
            size_t comp_len = strlen(component);

            if (out_len + comp_len + 2 > PATH_MAX) {
                errno = ENAMETOOLONG;
                free(work);
                return NULL;
            }

            if (out_len > 1) {
                strcat(resolved_path, "/");
            }

            strcat(resolved_path, component);
        }

        component = strtok(NULL, "/");
    }

    free(work);
    return resolved_path;
}

char * _PDCLIB_realpath( const char * path )
{
    /* TODO: PATH_MAX but that seems difficult to come by */
    char buffer[ 4096 ];
    char * resolved_name;

    if ( realpath( path, buffer ) == NULL )
    {
        return NULL;
    }

    /* Need to do our own alloc-and-copy here, as realpath()
       would be linked to the system malloc(), and if our
       fclose() would run our free() on someone else's memory,
       results are more interesting than we would like to see.
    */
    if ( ( resolved_name = malloc( strlen( buffer ) + 1 ) ) == NULL )
    {
        return NULL;
    }

    return strcpy( resolved_name, buffer );
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

int main( void )
{
    /* No test drivers. */
    return TEST_RESULTS;
}

#endif
