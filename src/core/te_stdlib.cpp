#include "te_stdlib.h"
#if !defined( _MSC_VER )
#include <unistd.h>
#endif
#include <fcntl.h>
#include <stdarg.h>

char* convert( unsigned int num, int base, char** output )
{
    static const char Representation[] = "0123456789ABCDEF";
    static char buffer[ 50 ];
    char* ptr = &buffer[ 49 ];
    *ptr = '\0';

    do
    {
        *--ptr = Representation[ num % base ];
        num /= base;
    } while (num != 0);

    while (*ptr != 0)
    {
        **output = *ptr;
        ++*output;
        ++ptr;
    }

    return(ptr);
}

void tePrint( const char* format, ... )
{
    va_list arg;
    va_start( arg, format );

    const char* ptr;
    char* s;
    int i;
    float f = 0;
    unsigned int u = 0;

    char* output = (char*)calloc( 260, 1 );
    char* outPtr = output;

    for (ptr = format; *ptr != '\0'; ++ptr)
    {
        while (*ptr != '%')
        {
            *outPtr++ = *ptr;
            ++ptr;

            if (*ptr == '\0')
                break;
}

        if (*ptr == '\0')
            break;

        ++ptr;

        switch (*ptr)
        {
        case 'x':
            u = va_arg( arg, unsigned int );
            *outPtr++ = '0';
            *outPtr++ = 'x';
            convert( u, 16, &outPtr );

            break;
        case 'd':
            i = va_arg( arg, int );
            if (i < 0)
            {
                i = -i;
                *outPtr++ = '-';
            }

            convert( i, 10, &outPtr );
            break;
        case 's':
            s = va_arg( arg, char* );
            break;
        case 'f':
            f = (float)va_arg( arg, double );

            if (f < 0)
            {
                *outPtr++ = '-';
            }

            int ii = (int)f < 0 ? -(int)f : (int)f;
            convert( ii, 10, &outPtr );
            *outPtr++ = '.';
            float dec = f - (int)f;
            if (dec < 0) dec = -dec;
            ii = (int)(dec * 10.0f);
            convert( ii, 10, &outPtr );

            break;
        }
    }

    va_end( arg );

#if _MSC_VER
    OutputDebugStringA( output );
#else
    int terminal = open( "/dev/tty", O_WRONLY );

    if (terminal < 0)
    {
        return;
    }

    dup2( terminal, STDOUT_FILENO );
    
    write( terminal, output, teStrlen( output ) );
#endif
}
