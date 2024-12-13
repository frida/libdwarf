/*
  Copyright (C) 2024 David Anderson. All Rights Reserved.

  This source is hereby placed in the Public Domain
  and may be used by anyone for any purpose.

*/

/*  Normally LIBDWARF_MALLOC is not defined.
    Only defined when researching malloc use in libdwarf.
    if defined here must also be defined in libdwarf_private.h */

#include "dwarf_local_malloc.h" /* for LIBDWARF_MALLOC */
#ifndef LIBDWARF_MALLOC
unsigned char phonyunused;
#else
#include <stdlib.h>
#include <stdio.h>
void * _libdwarf_malloc(size_t size);
void * _libdwarf_calloc(size_t n, size_t s);
void * _libdwarf_realloc(void * p, size_t s);
void   _libdwarf_free(void *p);
void   _libdwarf_finish(void);
static unsigned long total_alloc;
static unsigned long alloc_count;
static unsigned long largest_alloc;
static unsigned long free_count;
#define xmalloc    malloc
#define xrealloc   realloc
#define xcalloc    calloc
#define xfree      free

void * _libdwarf_malloc(size_t s)
{
    ++alloc_count;
    total_alloc += s;
    if ((unsigned long)s > largest_alloc) {
        largest_alloc = s;
        printf("dadebug line %d largest_alloc %lu\n",
            __LINE__,
            largest_alloc);
        fflush(stdout);
#if 0
        if (s > 1000000000) {
            abort();
        }
#endif
    }
    return xmalloc(s);
}
void * _libdwarf_calloc(size_t n, size_t s)
{
    ++alloc_count;
    total_alloc += n*s;
    if (n  > largest_alloc ) {
        largest_alloc = n*s;
        printf("dadebug line %d largest_alloc %lu "
            " n=%lu s=%lu\n",
            __LINE__,
            largest_alloc,
            (unsigned long)n,
            (unsigned long)s);
        fflush(stdout);
    }
    if (s  > largest_alloc ) {
        largest_alloc = n*s;
        printf("dadebug line %d largest_alloc %lu "
            " n=%lu s=%lu\n",
            __LINE__,
            largest_alloc,
            (unsigned long)n,
            (unsigned long)s);
        fflush(stdout);
    }
    if ((n * s) > largest_alloc) {
        largest_alloc = s*n;
        printf("dadebug line %d largest_alloc %lu "
            " n=%lu s=%lu\n",
            __LINE__,
            largest_alloc,
            (unsigned long)n,
            (unsigned long)s);
        fflush(stdout);
    }
    return xcalloc(n,s);
}
void * _libdwarf_realloc(void * p, size_t s)
{
    ++alloc_count;
    total_alloc += s;
    if ((s)  > largest_alloc) {
        largest_alloc = s;
        printf("dadebug line %d largest_alloc %lu\n",
            __LINE__,
            largest_alloc);
        fflush(stdout);
    }
    return xrealloc(p,s);
}
void _libdwarf_free(void *p)
{
    free_count++;
    xfree(p);
}
void _libdwarf_finish(void)
{
    printf("dadebug at finish total   alloc %lu\n",total_alloc);
    printf("dadebug at finish largest alloc %lu\n",largest_alloc);
    printf("dadebug at finish alloc count   %lu\n",alloc_count);
    printf("dadebug at finish    free count %lu\n",free_count);
    fflush(stdout);
    total_alloc = 0;
    largest_alloc = 0;
    alloc_count = 0;
    free_count = 0;
}
#endif /* LIBDWARF_MALLOC */