#include "file.h"
#include "scene.h"
#include <stdio.h>

void LoadUsdScene( const char* path )
{
    teFile file = teLoadFile( path );

}

void SaveUsdScene( const char* path )
{
    FILE* outFile = fopen( path, "wb" );

    const char header[] = { "#usda 1.0" };
    fwrite( header, sizeof( char ), sizeof( header ), outFile );

    fclose( outFile );
}
