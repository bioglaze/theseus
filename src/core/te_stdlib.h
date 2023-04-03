#pragma once

#if __APPLE__
#define teAssert( c ) if (!(c)) *(volatile int *)0 = 0
#endif

// TODO: Replace includes with own implementation of strstr, memcpy and malloc.
#include <string.h>
#include <stdlib.h>

static int teStrcmp( const char* s1, const char* s2 )
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }

    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static const char* teStrstr( const char* s1, const char* s2 )
{
    return strstr( s1, s2 );
}

static void teMemcpy( void* dst, const void* src, size_t size )
{
    memcpy( dst, src, size );
}

static void* teMalloc( size_t bytes )
{
    return malloc( bytes );
}

