#import <Metal/Metal.h>
#include "texture.h"
#include "file.h"
#include "te_stdlib.h"

void LoadTGA( const teFile& file, unsigned& outWidth, unsigned& outHeight, unsigned& outDataBeginOffset, unsigned& outBitsPerPixel );
bool LoadDDS( const teFile& fileContents, unsigned& outWidth, unsigned& outHeight, teTextureFormat& outFormat, unsigned& outMipLevelCount, unsigned( &outMipOffsets )[ 15 ] );

extern id<MTLDevice> gDevice;
extern id<MTLCommandQueue> gCommandQueue;

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
unsigned textureCount = 0;

static inline unsigned Max2( unsigned x, unsigned y ) noexcept
{
    return x > y ? x : y;
}

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
    else if (aeFormat == teTextureFormat::RGBA_sRGB)
    {
        return MTLPixelFormatRGBA8Unorm_sRGB;
    }
    else if (aeFormat == teTextureFormat::Depth32F_S8)
    {
        return MTLPixelFormatDepth32Float_Stencil8;
    }
    
    teAssert( !"unhandled pixel format" );
    
    return MTLPixelFormatRGBA8Unorm_sRGB;
}

id<MTLTexture> TextureGetMetalTexture( unsigned index )
{
    return textures[ index ].metalTexture;
}

unsigned TextureGetFlags( unsigned index )
{
    return textures[ index ].flags;
}

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName )
{
    teAssert( textureCount < 100 );

    const unsigned index = ++textureCount;

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

teTexture2D teLoadTexture( const teFile& file, unsigned flags, void* pixels, int pixelsWidth, int pixelsHeight, teTextureFormat pixelsFormat )
{
    teAssert( textureCount < 100 );
    teAssert( !(flags & teTextureFlags::UAV) );

    teTexture2D outTexture;
    outTexture.index = ++textureCount;
    outTexture.format = teTextureFormat::BGRA_sRGB;
    teTextureImpl& tex = textures[ outTexture.index ];
    tex.flags = flags;
    tex.format = MTLPixelFormatBGRA8Unorm_sRGB;
    
    if (file.data == nullptr && pixels == nullptr)
    {
        outTexture.index = 1;
        --textureCount;
        return outTexture;
    }
    
    unsigned multiplier = 1;
    unsigned bitsPerPixel = 0;
    unsigned offset = 0;
    
    if (strstr( file.path, ".tga" ) || strstr( file.path, ".TGA" ))
    {
        LoadTGA( file, tex.width, tex.height, offset, bitsPerPixel );
        multiplier = 4;

        MTLTextureDescriptor* stagingDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:tex.format
                                                           width:tex.width
                                                          height:tex.height
                                                       mipmapped:(flags & teTextureFlags::GenerateMips ? YES : NO)];
        id<MTLTexture> stagingTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];

        MTLRegion region = MTLRegionMake2D( 0, 0, tex.width, tex.height );
        [stagingTexture replaceRegion:region mipmapLevel:0 withBytes:&file.data[ offset ] bytesPerRow:tex.width * multiplier];

        stagingDescriptor.usage = MTLTextureUsageShaderRead;
        stagingDescriptor.storageMode = MTLStorageModePrivate;
        tex.metalTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];
        tex.metalTexture.label = [NSString stringWithUTF8String:file.path];

        id<MTLCommandBuffer> cmdBuffer = [gCommandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blitEncoder = [cmdBuffer blitCommandEncoder];
        [blitEncoder copyFromTexture:stagingTexture
                      sourceSlice:0
                      sourceLevel:0
                     sourceOrigin:MTLOriginMake( 0, 0, 0 )
                       sourceSize:MTLSizeMake( tex.width, tex.height, 1 )
                        toTexture:tex.metalTexture
                 destinationSlice:0
                 destinationLevel:0
                destinationOrigin:MTLOriginMake( 0, 0, 0 ) ];
        if ((flags & teTextureFlags::GenerateMips))
        {
            [blitEncoder generateMipmapsForTexture:tex.metalTexture];
        }
        [blitEncoder endEncoding];
        [cmdBuffer commit];
        [cmdBuffer waitUntilCompleted];
    }
    else if (pixels != nullptr)
    {
        tex.width = pixelsWidth;
        tex.height = pixelsHeight;
        tex.format = GetPixelFormat( pixelsFormat );
        outTexture.format = pixelsFormat;
        
        MTLTextureDescriptor* stagingDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:tex.format
                                                           width:tex.width
                                                          height:tex.height
                                                       mipmapped:(flags & teTextureFlags::GenerateMips ? YES : NO)];
        id<MTLTexture> stagingTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];

        const unsigned multiplier = 4; // FIXME: Other than RGBA formats probably need to change this.
        MTLRegion region = MTLRegionMake2D( 0, 0, tex.width, tex.height );
        [stagingTexture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:tex.width * multiplier];

        stagingDescriptor.usage = MTLTextureUsageShaderRead;
        stagingDescriptor.storageMode = MTLStorageModePrivate;
        tex.metalTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];
        tex.metalTexture.label = [NSString stringWithUTF8String:file.path];

        id<MTLCommandBuffer> cmdBuffer = [gCommandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blitEncoder = [cmdBuffer blitCommandEncoder];
        [blitEncoder copyFromTexture:stagingTexture
                      sourceSlice:0
                      sourceLevel:0
                     sourceOrigin:MTLOriginMake( 0, 0, 0 )
                       sourceSize:MTLSizeMake( tex.width, tex.height, 1 )
                        toTexture:tex.metalTexture
                 destinationSlice:0
                 destinationLevel:0
                destinationOrigin:MTLOriginMake( 0, 0, 0 ) ];
        if ((flags & teTextureFlags::GenerateMips))
        {
            [blitEncoder generateMipmapsForTexture:tex.metalTexture];
        }
        [blitEncoder endEncoding];
        [cmdBuffer commit];
        [cmdBuffer waitUntilCompleted];
    }
#if !TARGET_OS_IPHONE
    else if (strstr( file.path, ".dds" ) || strstr( file.path, ".DDS" ))
    {
        unsigned mipOffsets[ 15 ];
        LoadDDS( file, tex.width, tex.height, outTexture.format, tex.mipLevelCount, mipOffsets  );
        tex.format = GetPixelFormat( outTexture.format );
        multiplier = (outTexture.format == teTextureFormat::BC1 || outTexture.format == teTextureFormat::BC1_SRGB || outTexture.format == teTextureFormat::BC4U) ? 2 : 4;

        if (!(flags & teTextureFlags::GenerateMips))
        {
            tex.mipLevelCount = 1;
        }
        
        MTLTextureDescriptor* stagingDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:tex.format
                                                           width:tex.width
                                                          height:tex.height
                                                       mipmapped:(flags & teTextureFlags::GenerateMips ? YES : NO)];
        id<MTLTexture> stagingTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];

        for (unsigned mipIndex = 0; mipIndex < tex.mipLevelCount; ++mipIndex)
        {
            const unsigned mipWidth = Max2( tex.width >> mipIndex, 1 );
            const unsigned mipHeight = Max2( tex.height >> mipIndex, 1 );
            unsigned mipBytesPerRow = mipWidth * multiplier;
            
            if (mipBytesPerRow < 8)
            {
                mipBytesPerRow = 8;
            }
            
            MTLRegion region = MTLRegionMake2D( 0, 0, mipWidth, mipHeight );
            [stagingTexture replaceRegion:region mipmapLevel:mipIndex withBytes:&file.data[ mipOffsets[ mipIndex ] ] bytesPerRow:mipBytesPerRow];
        }

        stagingDescriptor.usage = MTLTextureUsageShaderRead;
        stagingDescriptor.storageMode = MTLStorageModePrivate;
        tex.metalTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];
        tex.metalTexture.label = [NSString stringWithUTF8String:file.path];

        id<MTLCommandBuffer> cmdBuffer = [gCommandQueue commandBuffer];
        cmdBuffer.label = @"BlitCommandBuffer";
        id<MTLBlitCommandEncoder> blitEncoder = [cmdBuffer blitCommandEncoder];

        for (unsigned mipIndex = 0; mipIndex < tex.mipLevelCount; ++mipIndex)
        {
            const unsigned mipWidth = Max2( tex.width >> mipIndex, 1 );
            const unsigned mipHeight = Max2( tex.height >> mipIndex, 1 );

            [blitEncoder copyFromTexture:stagingTexture
                              sourceSlice:0
                              sourceLevel:mipIndex
                             sourceOrigin:MTLOriginMake( 0, 0, 0 )
                               sourceSize:MTLSizeMake( mipWidth, mipHeight, 1 )
                                toTexture:tex.metalTexture
                         destinationSlice:0
                         destinationLevel:mipIndex
                        destinationOrigin:MTLOriginMake( 0, 0, 0 ) ];
        }
        
        [blitEncoder endEncoding];
        [cmdBuffer commit];
        [cmdBuffer waitUntilCompleted];
    }
#endif
    else
    {
        teAssert( !"Unknown texture extension!" );
    }
    
    return outTexture;
}

teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags )
{
    teAssert( textureCount < 100 );
    teAssert( !(flags & teTextureFlags::UAV) );

    teTextureCube outTexture;
    outTexture.index = ++textureCount;
    teTextureImpl& tex = textures[ outTexture.index ];
    tex.flags = flags;

#if !TARGET_OS_IPHONE
    if (strstr( negX.path, ".dds" ) || strstr( negX.path, ".DDS" ))
    {
        // This code assumes that all cube map faces have the same format/dimension.
        unsigned multiplier = 1;
        unsigned mipOffsets[ 15 ];
        
        LoadDDS( negX, tex.width, tex.height, outTexture.format, tex.mipLevelCount, mipOffsets );
        
        tex.format = GetPixelFormat( outTexture.format );
        multiplier = (outTexture.format == teTextureFormat::BC1 || outTexture.format == teTextureFormat::BC1_SRGB || outTexture.format == teTextureFormat::BC4U) ? 2 : 4;

        if (!(flags & teTextureFlags::GenerateMips))
        {
            tex.mipLevelCount = 1;
        }
        
        MTLTextureDescriptor* stagingDescriptor =
        [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:tex.format
                                                           size:tex.width
                                                       mipmapped:(flags & teTextureFlags::GenerateMips ? YES : NO)];
        id<MTLTexture> stagingTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];

        const unsigned char* datas[ 6 ] = { negX.data, posX.data, negY.data, posY.data, negZ.data, posZ.data };
        MTLRegion region = MTLRegionMake2D( 0, 0, tex.width, tex.width );
        
        for (int face = 0; face < 6; ++face)
        {
            [stagingTexture replaceRegion:region
                              mipmapLevel:0
                                    slice:face
                                withBytes:datas[ face ] + mipOffsets[ 0 ]
                              bytesPerRow:multiplier * tex.width
                            bytesPerImage:0];
        }

        stagingDescriptor.usage = MTLTextureUsageShaderRead;
        stagingDescriptor.storageMode = MTLStorageModePrivate;
        tex.metalTexture = [gDevice newTextureWithDescriptor:stagingDescriptor];
        tex.metalTexture.label = [NSString stringWithUTF8String:negX.path];

        for (unsigned mipIndex = 0; mipIndex < [stagingTexture mipmapLevelCount]; ++mipIndex)
        {
            for (unsigned faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                id<MTLCommandBuffer> cmdBuffer = [gCommandQueue commandBuffer];
                cmdBuffer.label = @"BlitCommandBuffer";
                id<MTLBlitCommandEncoder> blitEncoder = [cmdBuffer blitCommandEncoder];
                [blitEncoder copyFromTexture:stagingTexture
                                  sourceSlice:faceIndex
                                  sourceLevel:mipIndex
                                 sourceOrigin:MTLOriginMake( 0, 0, 0 )
                                   sourceSize:MTLSizeMake( tex.width >> mipIndex, tex.height >> mipIndex, 1 )
                                    toTexture:tex.metalTexture
                             destinationSlice:faceIndex
                             destinationLevel:mipIndex
                            destinationOrigin:MTLOriginMake( 0, 0, 0 ) ];
                [blitEncoder endEncoding];
                [cmdBuffer commit];
                [cmdBuffer waitUntilCompleted];
            }
        }
    }
#endif
    
    return outTexture;
}

