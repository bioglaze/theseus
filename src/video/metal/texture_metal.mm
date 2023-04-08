#import <Metal/Metal.h>
#include "texture.h"
#include "te_stdlib.h"

extern id<MTLDevice> gDevice;

struct teTextureImpl
{
    id<MTLTexture> metalTexture;
    MTLPixelFormat format;
    unsigned flags = 0;
    unsigned width = 0;
    unsigned height = 0;
    unsigned mipLevelCount = 1;
};

teTextureImpl textures[ 100 ];
unsigned textureCount = 1;

static MTLPixelFormat GetPixelFormat( teTextureFormat aeFormat )
{
#if !TARGET_OS_IPHONE
    if (aeFormat == teTextureFormat::BC1)
    {
        return MTLPixelFormatBC1_RGBA;
    }
    else if (aeFormat == teTextureFormat::BC1_SRGB)
    {
        return MTLPixelFormatBC1_RGBA_sRGB;
    }
    else if (aeFormat == teTextureFormat::BC2)
    {
        return MTLPixelFormatBC2_RGBA;
    }
    else if (aeFormat == teTextureFormat::BC2_SRGB)
    {
        return MTLPixelFormatBC2_RGBA_sRGB;
    }
    else if (aeFormat == teTextureFormat::BC3)
    {
        return MTLPixelFormatBC3_RGBA;
    }
    else if (aeFormat == teTextureFormat::BC3_SRGB)
    {
        return MTLPixelFormatBC3_RGBA_sRGB;
    }
    else if (aeFormat == teTextureFormat::BC4U)
    {
        return MTLPixelFormatBC4_RUnorm;
    }
    else if (aeFormat == teTextureFormat::BC4S)
    {
        return MTLPixelFormatBC4_RSnorm;
    }
    else if (aeFormat == teTextureFormat::BC5U)
    {
        return MTLPixelFormatBC5_RGUnorm;
    }
    else if (aeFormat == teTextureFormat::BC5S)
    {
        return MTLPixelFormatBC5_RGSnorm;
    }
    else
#endif
    if (aeFormat == teTextureFormat::R32G32F)
    {
        return MTLPixelFormatRG32Float;
    }
    if (aeFormat == teTextureFormat::R32F)
    {
        return MTLPixelFormatR32Float;
    }
    else if (aeFormat == teTextureFormat::Depth32F)
    {
        return MTLPixelFormatDepth32Float;
    }
    else if (aeFormat == teTextureFormat::BGRA_sRGB)
    {
        return MTLPixelFormatBGRA8Unorm_sRGB;
    }

    return MTLPixelFormatRGBA8Unorm_sRGB;
}

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName )
{
    teAssert( textureCount < 100 );

    const unsigned index = textureCount++;

    teTextureImpl& tex = textures[ index ];
    tex.width = width;
    tex.height = height;
    tex.flags = flags;

    teTexture2D outTex;
    outTex.index = index;
    outTex.format = format;

    MTLPixelFormat pixelFormat = GetPixelFormat( format );
    
    MTLTextureDescriptor* textureDescriptor =
    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                       width:width
                                                      height:height
                                                   mipmapped:NO];
    textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    if (flags & teTextureFlags::RenderTexture)
    {
        textureDescriptor.usage |= MTLTextureUsageRenderTarget;
    }
    
    if (flags & teTextureFlags::UAV)
    {
        textureDescriptor.usage |= MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    }

    textureDescriptor.storageMode = MTLStorageModePrivate;
    tex.metalTexture = [gDevice newTextureWithDescriptor:textureDescriptor];
    tex.metalTexture.label = [NSString stringWithUTF8String:debugName];

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

