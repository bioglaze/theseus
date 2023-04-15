#include "file.h"
#include <stdio.h>

#if __APPLE__
const char* GetFullPath( const char* fileName );
#else
const char* GetFullPath( const char* fileName )
{
    return fileName;
}
#endif

teFile teLoadFile( const char* path )
{
    teFile outFile;
    
    if (path && *path == 0)
    {
        return outFile;
    }

    FILE* file = fopen( GetFullPath( path ), "rb" );

    if (file)
    {    
        fseek( file, 0, SEEK_END );
        auto length = ftell( file );
        fseek( file, 0, SEEK_SET );
        outFile.data = new unsigned char[ length ];
        outFile.size = (unsigned)length;

        for (unsigned i = 0; i < 260; ++i)
        {
            outFile.path[ i ] = path[ i ];

            if (path[ i ] == 0)
            {
                break;
            }
        }
        
        fread( outFile.data, 1, length, file );    
        fclose( file );
    }
    else
    {
        printf( "Could not open file %s\n", path );
    }
    
    return outFile;
}
