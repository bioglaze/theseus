#import <MetalKit/MetalKit.h>
#include "buffer.h"
#include "material.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"

struct Renderer
{
    id<MTLDevice> device;
    id<MTLBuffer> uniformBuffer;
    id<MTLRenderCommandEncoder> renderEncoder;
    
    id<MTLSamplerState> anisotropicRepeat;
    id<MTLSamplerState> anisotropicClamp;
    id<MTLSamplerState> linearRepeat;
    id<MTLSamplerState> linearClamp;
    id<MTLSamplerState> nearestRepeat;
    id<MTLSamplerState> nearestClamp;

    unsigned indexCounter = 0;
    unsigned uvCounter = 0;
    unsigned positionCounter = 0;
    
    unsigned width = 0;
    unsigned height = 0;
};

Renderer renderer;

id<MTLLibrary> defaultLibrary;

void teCreateRenderer( unsigned swapInterval, void* windowHandle, unsigned width, unsigned height )
{
    renderer.device = MTLCreateSystemDefaultDevice();

    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];

    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.maxAnisotropy = 1;
    samplerDescriptor.label = @"linear repeat";
    renderer.linearRepeat = [renderer.device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.maxAnisotropy = 1;
    samplerDescriptor.label = @"linear clamp";
    renderer.linearClamp = [renderer.device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.maxAnisotropy = 8;
    samplerDescriptor.label = @"anisotropic8 repeat";
    renderer.anisotropicRepeat = [renderer.device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.maxAnisotropy = 8;
    samplerDescriptor.label = @"anisotropic8 clamp";
    renderer.anisotropicClamp = [renderer.device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterNearest;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.maxAnisotropy = 1;
    samplerDescriptor.label = @"nearest repeat";
    renderer.nearestRepeat = [renderer.device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterNearest;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.maxAnisotropy = 1;
    samplerDescriptor.label = @"nearest clamp";
    renderer.nearestClamp = [renderer.device newSamplerStateWithDescriptor:samplerDescriptor];

    defaultLibrary = [renderer.device newDefaultLibrary];
    
    renderer.width = width;
    renderer.height = height;
}

void PushGroupMarker( const char* name )
{
    NSString* fName = [NSString stringWithUTF8String: name];
    [renderer.renderEncoder pushDebugGroup:fName];
}

void PopGroupMarker()
{
    [renderer.renderEncoder popDebugGroup];
}

unsigned AddIndices( const unsigned short* indices, unsigned bytes )
{
    if (indices)
    {
        //UpdateStagingBuffer( renderer.staticMeshIndexStagingBuffer, indices, bytes, renderer.indexCounter );
    }

    renderer.indexCounter += bytes;
    return renderer.indexCounter - bytes;
}

unsigned AddUVs( const float* uvs, unsigned bytes )
{
    if (uvs)
    {
        //UpdateStagingBuffer( renderer.staticMeshUVStagingBuffer, uvs, bytes, renderer.uvCounter );
    }

    renderer.uvCounter += bytes;
    return renderer.uvCounter - bytes;
}

unsigned AddPositions( const float* positions, unsigned bytes )
{
    if (positions)
    {
        //UpdateStagingBuffer( renderer.staticMeshPositionStagingBuffer, positions, bytes, renderer.positionCounter );
    }

    renderer.positionCounter += bytes;
    return renderer.positionCounter - bytes;
}

void UpdateUBO( const float localToClip0[ 16 ], const float localToClip1[ 16 ], const float localToView0[ 16 ], const float localToView1[ 16 ] )
{
    
}

void BeginRendering( teTexture2D& color, teTexture2D& depth )
{
    
}

void EndRendering( teTexture2D& color )
{
    
}

void Draw( const teShader& shader, unsigned positionOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, teTextureFormat colorFormat, teTextureFormat depthFormat, unsigned textureIndex )
{
    
}
