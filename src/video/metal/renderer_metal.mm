#import <MetalKit/MetalKit.h>
#include "buffer.h"
#include "material.h"
#include "matrix.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "te_stdlib.h"

Buffer CreateBuffer( id<MTLDevice> device, unsigned dataBytes, bool isStaging );
unsigned BufferGetSizeBytes( const Buffer& buffer );
id<MTLBuffer> BufferGetBuffer( const Buffer& buffer );

struct PerObjectUboStruct
{
    Matrix localToClip[ 2 ];
    Matrix localToView[ 2 ];
};

struct FrameResource
{
    id<MTLCommandBuffer> commandBuffer;
    id<MTLBuffer> uniformBuffer;
    unsigned uboOffset = 0;
};

struct Renderer
{
    id<MTLDevice> device;
    
    id<MTLRenderCommandEncoder> renderEncoder;
    id<MTLCommandQueue> commandQueue;
    FrameResource frameResources[ 2 ];
    teTextureFormat colorFormat = teTextureFormat::BGRA_sRGB;
    
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
    
    Buffer staticMeshIndexBuffer;
    Buffer staticMeshIndexStagingBuffer;
    Buffer staticMeshPositionBuffer;
    Buffer staticMeshPositionStagingBuffer;
    Buffer staticMeshUVStagingBuffer;
    Buffer staticMeshUVBuffer;
};

Renderer renderer;

id<MTLLibrary> defaultLibrary;
id<CAMetalDrawable> gDrawable;
id<MTLDevice> gDevice;

void teCreateRenderer( unsigned swapInterval, void* windowHandle, unsigned width, unsigned height )
{
    renderer.device = MTLCreateSystemDefaultDevice();
    gDevice = renderer.device;
    
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
    
    renderer.commandQueue = [renderer.device newCommandQueue];
    
    const unsigned bufferBytes = 1024 * 1024 * 500;
    
    renderer.staticMeshIndexBuffer = CreateBuffer( renderer.device, bufferBytes, false );
    renderer.staticMeshIndexStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true );
    renderer.staticMeshPositionBuffer = CreateBuffer( renderer.device, bufferBytes, false );
    renderer.staticMeshPositionStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true );
    renderer.staticMeshUVBuffer = CreateBuffer( renderer.device, bufferBytes, false );
    renderer.staticMeshUVStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true );
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

void UpdateStagingBuffer( const Buffer& buffer, const void* data, unsigned dataBytes, unsigned offset )
{
    const unsigned dataBytesNextMultipleOf4 = ((dataBytes + 3) / 4) * 4;
    
    uint8_t* bufferPointer = (uint8_t *)[BufferGetBuffer( buffer ) contents];

    teMemcpy( bufferPointer, data, dataBytesNextMultipleOf4 );
}

unsigned AddIndices( const unsigned short* indices, unsigned bytes )
{
    if (indices)
    {
        UpdateStagingBuffer( renderer.staticMeshIndexStagingBuffer, indices, bytes, renderer.indexCounter );
    }

    renderer.indexCounter += bytes;
    return renderer.indexCounter - bytes;
}

unsigned AddUVs( const float* uvs, unsigned bytes )
{
    if (uvs)
    {
        UpdateStagingBuffer( renderer.staticMeshUVStagingBuffer, uvs, bytes, renderer.uvCounter );
    }

    renderer.uvCounter += bytes;
    return renderer.uvCounter - bytes;
}

unsigned AddPositions( const float* positions, unsigned bytes )
{
    if (positions)
    {
        UpdateStagingBuffer( renderer.staticMeshPositionStagingBuffer, positions, bytes, renderer.positionCounter );
    }

    renderer.positionCounter += bytes;
    return renderer.positionCounter - bytes;
}

void UpdateUBO( const float localToClip0[ 16 ], const float localToClip1[ 16 ], const float localToView0[ 16 ], const float localToView1[ 16 ] )
{
    PerObjectUboStruct uboStruct = {};
    uboStruct.localToClip[ 0 ].InitFrom( localToClip0 );
    uboStruct.localToClip[ 1 ].InitFrom( localToClip1 );
    uboStruct.localToView[ 0 ].InitFrom( localToView0 );
    uboStruct.localToView[ 1 ].InitFrom( localToView1 );

    id<MTLBuffer> uniformBuffer = renderer.frameResources[ 0 ].uniformBuffer;
    uint8_t* bufferPointer = (uint8_t*)[uniformBuffer contents] + renderer.frameResources[ 0 ].uboOffset;

    memcpy( bufferPointer, &uboStruct, sizeof( PerObjectUboStruct ) );
#if !TARGET_OS_IPHONE
    [uniformBuffer didModifyRange:NSMakeRange( renderer.frameResources[ 0 ].uboOffset, sizeof( PerObjectUboStruct ) )];
#endif
}

void teBeginFrame()
{
    renderer.frameResources[ 0 ].uboOffset = 0;
}

void SetDrawable( id< CAMetalDrawable > drawable )
{
    gDrawable = drawable;
}

void teEndFrame()
{
    [renderer.frameResources[ 0 ].commandBuffer presentDrawable:gDrawable];
    [renderer.frameResources[ 0 ].commandBuffer commit];
}

void BeginRendering( teTexture2D& color, teTexture2D& depth )
{
    
}

void EndRendering( teTexture2D& color )
{
    
}

void teBeginSwapchainRendering( teTexture2D& color )
{
    
}

void teEndSwapchainRendering()
{
    
}

void CopyBuffer( const Buffer& source, const Buffer& destination )
{
    teAssert( BufferGetSizeBytes( source ) <= BufferGetSizeBytes( destination ) );
    
    id <MTLCommandBuffer> cmd_buffer =     [renderer.commandQueue commandBuffer];
    cmd_buffer.label = @"BlitCommandBuffer";
    id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
    [blit_encoder copyFromBuffer:BufferGetBuffer( source )
                    sourceOffset:0
                        toBuffer:BufferGetBuffer( destination )
               destinationOffset:0
                            size:BufferGetSizeBytes( source )];
    [blit_encoder endEncoding];
    [cmd_buffer commit];
    [cmd_buffer waitUntilCompleted];
}

void teFinalizeMeshBuffers()
{
    CopyBuffer( renderer.staticMeshIndexStagingBuffer, renderer.staticMeshIndexBuffer );
    CopyBuffer( renderer.staticMeshUVStagingBuffer, renderer.staticMeshUVBuffer );
    CopyBuffer( renderer.staticMeshPositionStagingBuffer, renderer.staticMeshPositionBuffer );
}

void Draw( const teShader& shader, unsigned positionOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, teTextureFormat colorFormat, teTextureFormat depthFormat, unsigned textureIndex )
{
    renderer.frameResources[ 0 ].uboOffset += sizeof( PerObjectUboStruct );
}

void teDrawFullscreenTriangle( teShader& shader, teTexture2D& texture )
{
    Draw( shader, 0, 3, 0, teBlendMode::Off, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, renderer.colorFormat, teTextureFormat::Depth32F, texture.index );
}
