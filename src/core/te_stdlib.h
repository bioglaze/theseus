#pragma once

// TODO: Replace includes with own implementation of memcpy and malloc.
#include <string.h>
#include <stdlib.h>

#if _MSC_VER
#define teAssert( c ) if (!(c)) __debugbreak()
#else
//#define teAssert( c ) if (!(c)) *(volatile int *)0 = 0
#define teAssert( c ) if (!(c)) __builtin_trap()
#endif

#if _DEBUG
#define VK_CHECK( x ) { VkResult res = (x); teAssert( res == VK_SUCCESS ); }
#else
#define VK_CHECK( x ) x
#endif

#ifndef TE_ALLOCA
#if defined( __APPLE__ ) || defined( __GLIBC__ )
#include <alloca.h>
#elif defined( _MSC_VER )
#include <malloc.h>
#ifndef alloca
#define alloca _alloca
#endif // alloca
#else
#include <stdlib.h>
#endif // _MSC_VER
#define TE_ALLOCA alloca
#endif // TE_ALLOCA

void tePrint( const char* format, ... );

static int teStrcmp( const char* s1, const char* s2 )
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }

    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static unsigned teStrlen( const char* str )
{
    unsigned len = 0;

    while (str[ len ] != 0)
    {
        ++len;
    }

    return len;
}

static const char* teStrstr( const char* s1, const char* s2 )
{
    int len = teStrlen( s2 );
    const char* ref = s2;

    while (*s1 && *ref)
    {
        if (*s1++ == *ref)
        {
            ref++;
        }
        if (!*ref)
        {
            return (s1 - len);
        }
        if (len == (ref - s2))
        {
            ref = s2;
        }
    }

    return nullptr;
}

static void teMemcpy( void* dst, const void* src, size_t size )
{
    char* x = (char*)dst;
    char* y = (char*)src;
    teAssert( !((x <= y && x + size > y) || (y <= x && y + size > x)) ); // overlap test

    memcpy( dst, src, size );
}

static void* teMalloc( size_t bytes )
{
    teAssert( bytes > 0 );

    return malloc( bytes );
}

static void teFree( void* ptr )
{
    free( ptr );
}
