#include "te_stdlib.h"
#if !defined( _MSC_VER )
#include <unistd.h>
#endif
#include <fcntl.h>
#include <stdarg.h>

void tePrint( const char* format, ... )
{
#if _MSC_VER
    OutputDebugStringA( format );
#else
    int terminal = open( "/dev/tty", O_WRONLY );

    if (terminal < 0)
    {
        return;
    }

    dup2( terminal, STDOUT_FILENO );

    va_list arg;
    va_start( arg, format );

    // TODO: parse args.
    
    write( terminal, format, teStrlen( format ) );

    va_end( arg );
#endif
}
