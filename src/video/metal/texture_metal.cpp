#include <Metal/Metal.hpp>
#include "texture.h"
#include "file.h"
#include "te_stdlib.h"

void LoadTGA( const teFile& file, unsigned& outWidth, unsigned& outHeight, unsigned& outDataBeginOffset, unsigned& outBitsPerPixel );
bool LoadDDS( const teFile& fileContents, unsigned& outWidth, unsigned& outHeight, teTextureFormat& outFormat, unsigned& outMipLevelCount, unsigned( &outMipOffsets )[ 15 ] );

extern MTL::Device* gDevice;
extern MTL::CommandQueue* gCommandQueue;

struct teTextureImpl
{
    MTL::Texture* metalTexture;
    MTL::PixelFormat format;
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

static MTL::PixelFormat GetPixelFormat( teTextureFormat aeFormat )
{
#if !TARGET_OS_IPHONE
    if (aeFormat == teTextureFormat::BC1)
    {
        return MTL::PixelFormatBC1_RGBA;
    }
    else if (aeFormat == teTextureFormat::BC1_SRGB)
    {
        return MTL::PixelFormatBC1_RGBA_sRGB;
    }
    else if (aeFormat == teTextureFormat::BC2)
    {
        return MTL::PixelFormatBC2_RGBA;
    }
    else if (aeFormat == teTextureFormat::BC2_SRGB)
    {
        return MTL::PixelFormatBC2_RGBA_sRGB;
    }
    else if (aeFormat == teTextureFormat::BC3)
    {
        return MTL::PixelFormatBC3_RGBA;
    }
    else if (aeFormat == teTextureFormat::BC3_SRGB)
    {
        return MTL::PixelFormatBC3_RGBA_sRGB;
    }
    else if (aeFormat == teTextureFormat::BC4U)
    {
        return MTL::PixelFormatBC4_RUnorm;
    }
    else if (aeFormat == teTextureFormat::BC4S)
    {
        return MTL::PixelFormatBC4_RSnorm;
    }
    else if (aeFormat == teTextureFormat::BC5U)
    {
        return MTL::PixelFormatBC5_RGUnorm;
    }
    else if (aeFormat == teTextureFormat::BC5S)
    {
        return MTL::PixelFormatBC5_RGSnorm;
    }
    else
#endif
    if (aeFormat == teTextureFormat::R32G32F)
    {
        return MTL::PixelFormatRG32Float;
    }
    else if (aeFormat == teTextureFormat::R32F)
    {
        return MTL::PixelFormatR32Float;
    }
    else if (aeFormat == teTextureFormat::R32G32B32A32F)
    {
        return MTL::PixelFormatRGBA32Float;
    }
    else if (aeFormat == teTextureFormat::Depth32F)
    {
        return MTL::PixelFormatDepth32Float;
    }
    else if (aeFormat == teTextureFormat::BGRA_sRGB)
    {
        return MTL::PixelFormatBGRA8Unorm_sRGB;
    }
    else if (aeFormat == teTextureFormat::RGBA_sRGB)
    {
        return MTL::PixelFormatRGBA8Unorm_sRGB;
    }
    else if (aeFormat == teTextureFormat::Depth32F_S8)
    {
        return MTL::PixelFormatDepth32Float_Stencil8;
    }
    
    teAssert( !"unhandled pixel format" );
    
    return MTL::PixelFormatRGBA8Unorm_sRGB;
}

MTL::Texture* TextureGetMetalTexture( unsigned index )
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

    MTL::PixelFormat pixelFormat = GetPixelFormat( format );
    
    MTL::TextureDescriptor* textureDescriptor =
    MTL::TextureDescriptor::texture2DDescriptor( pixelFormat,
                                                       width,
                                                      height,
                                                   false);
    textureDescriptor->setUsage( MTL::TextureUsageShaderRead );
    
    if (flags & teTextureFlags::RenderTexture)
    {
        textureDescriptor->setUsage( MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget );
    }
    
    if (flags & teTextureFlags::UAV)
    {
        textureDescriptor->setUsage( textureDescriptor->usage() | MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite );
    }

    textureDescriptor->setStorageMode( MTL::StorageModePrivate );
    tex.metalTexture = gDevice->newTexture( textureDescriptor );
    tex.metalTexture->setLabel( NS::String::string( debugName, NS::StringEncoding::UTF8StringEncoding ) );

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
    tex.format = MTL::PixelFormatBGRA8Unorm_sRGB;
    
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

        MTL::TextureDescriptor* stagingDescriptor =
        MTL::TextureDescriptor::texture2DDescriptor( tex.format,
                                                           tex.width,
                                                          tex.height,
                                                       (flags & teTextureFlags::GenerateMips ? true : false) );
        MTL::Texture* stagingTexture = gDevice->newTexture( stagingDescriptor );

        MTL::Region region = MTL::Region::Make2D( 0, 0, tex.width, tex.height );
        stagingTexture->replaceRegion( region, 0, &file.data[ offset ], tex.width * multiplier );

        stagingDescriptor->setUsage( MTL::TextureUsageShaderRead );
        stagingDescriptor->setStorageMode( MTL::StorageModePrivate );
        tex.metalTexture = gDevice->newTexture( stagingDescriptor );
        tex.metalTexture->setLabel( NS::String::string( file.path, NS::StringEncoding::UTF8StringEncoding ) );

        MTL::CommandBuffer* cmdBuffer = gCommandQueue->commandBuffer();
        MTL::BlitCommandEncoder* blitEncoder = cmdBuffer->blitCommandEncoder();
        blitEncoder->copyFromTexture( stagingTexture,
                      0,
                      0,
                      MTL::Origin::Make( 0, 0, 0 ),
                      MTL::Size::Make( tex.width, tex.height, 1 ),
                      tex.metalTexture,
                      0,
                      0,
                      MTL::Origin::Make( 0, 0, 0 ) );
        if ((flags & teTextureFlags::GenerateMips))
        {
            blitEncoder->generateMipmaps( tex.metalTexture );
        }
        blitEncoder->endEncoding();
        cmdBuffer->commit();
        cmdBuffer->waitUntilCompleted();
    }
    else if (pixels != nullptr)
    {
        tex.width = pixelsWidth;
        tex.height = pixelsHeight;
        tex.format = GetPixelFormat( pixelsFormat );
        outTexture.format = pixelsFormat;
        
        MTL::TextureDescriptor* stagingDescriptor =
        MTL::TextureDescriptor::texture2DDescriptor( tex.format,
                                                           tex.width,
                                                          tex.height,
                                                       (flags & teTextureFlags::GenerateMips ? true : false));
        MTL::Texture* stagingTexture = gDevice->newTexture( stagingDescriptor );

        multiplier = 4; // FIXME: Other than RGBA formats probably need to change this.
        MTL::Region region = MTL::Region::Make2D( 0, 0, tex.width, tex.height );
        stagingTexture->replaceRegion( region, 0, pixels, tex.width * multiplier );

        stagingDescriptor->setUsage( MTL::TextureUsageShaderRead );
        stagingDescriptor->setStorageMode( MTL::StorageModePrivate );
        tex.metalTexture = gDevice->newTexture( stagingDescriptor );
        tex.metalTexture->setLabel( NS::String::string( file.path, NS::StringEncoding::UTF8StringEncoding ) );

        MTL::CommandBuffer* cmdBuffer = gCommandQueue->commandBuffer();
        MTL::BlitCommandEncoder* blitEncoder = cmdBuffer->blitCommandEncoder();
        blitEncoder->copyFromTexture( stagingTexture,
                      0,
                      0,
                      MTL::Origin::Make( 0, 0, 0 ),
                      MTL::Size::Make( tex.width, tex.height, 1 ),
                      tex.metalTexture,
                      0,
                      0,
                      MTL::Origin::Make( 0, 0, 0 ) );
        if ((flags & teTextureFlags::GenerateMips))
        {
            blitEncoder->generateMipmaps( tex.metalTexture );
        }
        blitEncoder->endEncoding();
        cmdBuffer->commit();
        cmdBuffer->waitUntilCompleted();
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
        
        MTL::TextureDescriptor* stagingDescriptor =
        MTL::TextureDescriptor::texture2DDescriptor( tex.format,
                                                           tex.width,
                                                          tex.height,
                                                       (flags & teTextureFlags::GenerateMips ? true : false ) );
        MTL::Texture* stagingTexture = gDevice->newTexture( stagingDescriptor );

        for (unsigned mipIndex = 0; mipIndex < tex.mipLevelCount; ++mipIndex)
        {
            const unsigned mipWidth = Max2( tex.width >> mipIndex, 1 );
            const unsigned mipHeight = Max2( tex.height >> mipIndex, 1 );
            unsigned mipBytesPerRow = mipWidth * multiplier;
            
            if (mipBytesPerRow < 16 && (tex.format == MTL::PixelFormatBC5_RGUnorm || tex.format == MTL::PixelFormatBC5_RGSnorm))
            {
                mipBytesPerRow = 16;
            }

            if (mipBytesPerRow < 8)
            {
                mipBytesPerRow = 8;
            }
            
            MTL::Region region = MTL::Region::Make2D( 0, 0, mipWidth, mipHeight );
            stagingTexture->replaceRegion( region, mipIndex, &file.data[ mipOffsets[ mipIndex ] ], mipBytesPerRow );
        }

        stagingDescriptor->setUsage( MTL::TextureUsageShaderRead );
        stagingDescriptor->setStorageMode( MTL::StorageModePrivate );
        tex.metalTexture = gDevice->newTexture( stagingDescriptor );
        tex.metalTexture->setLabel( NS::String::string( file.path, NS::StringEncoding::UTF8StringEncoding ) );

        MTL::CommandBuffer* cmdBuffer = gCommandQueue->commandBuffer();
        cmdBuffer->setLabel( NS::String::string( "BlitCommandBuffer", NS::StringEncoding::UTF8StringEncoding ) );

        MTL::BlitCommandEncoder* blitEncoder = cmdBuffer->blitCommandEncoder();

        for (unsigned mipIndex = 0; mipIndex < tex.mipLevelCount; ++mipIndex)
        {
            const unsigned mipWidth = Max2( tex.width >> mipIndex, 1 );
            const unsigned mipHeight = Max2( tex.height >> mipIndex, 1 );

            blitEncoder->copyFromTexture( stagingTexture,
                              0,
                              mipIndex,
                             MTL::Origin::Make( 0, 0, 0 ),
                               MTL::Size::Make( mipWidth, mipHeight, 1 ),
                                tex.metalTexture,
                         0,
                         mipIndex,
                        MTL::Origin::Make( 0, 0, 0 ) );
        }
        
        blitEncoder->endEncoding();
        cmdBuffer->commit();
        cmdBuffer->waitUntilCompleted();
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
        
        MTL::TextureDescriptor* stagingDescriptor =
        MTL::TextureDescriptor::textureCubeDescriptor( tex.format,
                                                           tex.width,
                                                       (flags & teTextureFlags::GenerateMips ? true : false));
        MTL::Texture* stagingTexture = gDevice->newTexture( stagingDescriptor );

        const unsigned char* datas[ 6 ] = { negX.data, posX.data, negY.data, posY.data, negZ.data, posZ.data };
        MTL::Region region = MTL::Region::Make2D( 0, 0, tex.width, tex.width );
        
        for (int face = 0; face < 6; ++face)
        {
            stagingTexture->replaceRegion( region, 0, face, datas[ face ] + mipOffsets[ 0 ], multiplier * tex.width, 0 );
        }

        stagingDescriptor->setUsage( MTL::TextureUsageShaderRead );
        stagingDescriptor->setStorageMode( MTL::StorageModePrivate );
        tex.metalTexture = gDevice->newTexture( stagingDescriptor );
        //tex.metalTexture.label = [NSString stringWithUTF8String:negX.path];

        for (unsigned mipIndex = 0; mipIndex < stagingTexture->mipmapLevelCount(); ++mipIndex)
        {
            for (unsigned faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                MTL::CommandBuffer* cmdBuffer = gCommandQueue->commandBuffer();
                //cmdBuffer.label = @"BlitCommandBuffer";
                MTL::BlitCommandEncoder* blitEncoder = cmdBuffer->blitCommandEncoder();
                blitEncoder->copyFromTexture( stagingTexture,
                                  faceIndex,
                                  mipIndex,
                                 MTL::Origin::Make( 0, 0, 0 ),
                                   MTL::Size::Make( tex.width >> mipIndex, tex.height >> mipIndex, 1 ),
                                    tex.metalTexture,
                             faceIndex,
                             mipIndex,
                            MTL::Origin::Make( 0, 0, 0 ) );
                blitEncoder->endEncoding();
                cmdBuffer->commit();
                cmdBuffer->waitUntilCompleted();
            }
        }
    }
#endif
    
    return outTexture;
}

