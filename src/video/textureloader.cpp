#include <stdio.h>
#include "file.h"

void teMemcpy( void* dst, const void* src, size_t size );
void* teMalloc( size_t bytes );

void LoadTGA( const teFile& file, unsigned& outWidth, unsigned& outHeight, unsigned& outDataBeginOffset, unsigned& outBitsPerPixel, unsigned char** outPixelData )
{
    unsigned char* data = (unsigned char*)file.data;

    unsigned offs = 0;
    offs++;
    offs++;
    int imageType = data[ offs++ ];

    if (imageType != 2)
    {
        printf( "Incompatible .tga file: %s. Must be truecolor and not use RLE.\n", file.path );
        return;
    }

    offs += 5; // colorSpec
    offs += 4; // specBegin

    unsigned width = data[ offs ] | (data[ offs + 1 ] << 8);
    ++offs;

    unsigned height = data[ offs + 1 ] | (data[ offs + 2 ] << 8);

    ++offs;

    outWidth = width;
    outHeight = height;

    offs += 4; // specEnd
    outBitsPerPixel = data[ offs - 2 ];

    *outPixelData = (unsigned char*)teMalloc( width * height * 4 );
    teMemcpy( *outPixelData, data, width * outBitsPerPixel / 8 );

    const int topLeft = data[ offs - 1 ] & 32;

    if (topLeft != 32)
    {
        printf( "%s image origin is not top left.\n", file.path );
    }

    outDataBeginOffset = offs;
}
