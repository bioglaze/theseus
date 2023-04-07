#import <Metal/Metal.h>
#include "texture.h"
#include "te_stdlib.h"

struct teTextureImpl
{
    MTLPixelFormat format;
    unsigned flags = 0;
    unsigned width = 0;
    unsigned height = 0;
    unsigned mipLevelCount = 1;
};

teTextureImpl textures[ 100 ];
unsigned textureCount = 1;

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName )
{
    const unsigned index = textureCount++;

    teTextureImpl& tex = textures[ index ];
    tex.width = width;
    tex.height = height;
    tex.flags = flags;

    teTexture2D outTex;
    outTex.index = index;
    outTex.format = format;

    return outTex;
}

void teTextureGetDimension( teTexture2D texture, unsigned& outWidth, unsigned& outHeight )
{
    outWidth = textures[ texture.index ].width;
    outHeight = textures[ texture.index ].height;
}

teTexture2D teLoadTexture( const struct teFile& file, unsigned flags )
{
    teAssert( textureCount < 100 );
    teAssert( !(flags & teTextureFlags::UAV) );

    teTexture2D outTexture;
    outTexture.index = textureCount++;
    teTextureImpl& tex = textures[ outTexture.index ];
    //tex.filter = filter;
    tex.flags = flags;

    return outTexture;
}

teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags, teTextureFilter filter )
{
    teAssert( textureCount < 100 );
    teAssert( !(flags & teTextureFlags::UAV) );

    teTextureCube outTexture;
    outTexture.index = textureCount++;
    teTextureImpl& tex = textures[ outTexture.index ];
    //tex.filter = filter;
    tex.flags = flags;

    return outTexture;
}

