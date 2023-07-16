#import <MetalKit/MetalKit.h>
#import <Foundation/Foundation.h>
#include "buffer.h"
#include "camera.h"
#include "material.h"
#include "matrix.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "te_stdlib.h"

Buffer CreateBuffer( id<MTLDevice> device, unsigned dataBytes, bool isStaging );
unsigned BufferGetSizeBytes( const Buffer& buffer );
id<MTLBuffer> BufferGetBuffer( const Buffer& buffer );
id<MTLFunction> teShaderGetVertexProgram( const teShader& shader );
id<MTLFunction> teShaderGetPixelProgram( const teShader& shader );
id<MTLTexture> TextureGetMetalTexture( unsigned index );

static const unsigned MaxPSOs = 100;

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

struct PSO
{
    id<MTLRenderPipelineState> pso;
    id<MTLFunction> vertexFunction;
    id<MTLFunction> pixelFunction;
    MTLPixelFormat format = MTLPixelFormatBGRA8Unorm_sRGB;
    teBlendMode blendMode = teBlendMode::Off;
    teTopology topology = teTopology::Triangles;
};

struct Renderer
{
    id<MTLDevice> device;
    id<MTLRenderCommandEncoder> renderEncoder;
    id<MTLCommandQueue> commandQueue;
    MTLRenderPassDescriptor* renderPassDescriptorFBO;
    FrameResource frameResources[ 2 ];
    teTextureFormat colorFormat = teTextureFormat::BGRA_sRGB;
    PSO psos[ MaxPSOs ];
    
    id<MTLDepthStencilState> depthStateGreaterWriteOn;
    id<MTLDepthStencilState> depthStateGreaterWriteOff;
    id<MTLDepthStencilState> depthStateNoneWriteOff;

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
    
    Buffer uiVertexBuffer;
    Buffer uiIndexBuffer;
    float* uiVertices = nullptr;
    uint16_t* uiIndices = nullptr;

    teTexture2D defaultTexture2D;
};

Renderer renderer;

id<MTLLibrary> defaultLibrary;
id<CAMetalDrawable> gDrawable;
id<MTLDevice> gDevice;
id<MTLCommandQueue> gCommandQueue;
MTLRenderPassDescriptor* renderPassDescriptor; // This comes from the application
id<MTLCommandBuffer> gCommandBuffer; // This is used by the application.

// TODO: Move into a better place.
const char* GetFullPath( const char* fileName )
{
    NSBundle *b = [NSBundle mainBundle];
    NSString *dir = [b resourcePath];
    NSString* fName = [NSString stringWithUTF8String: "/"];
    NSString* fPath = [NSString stringWithUTF8String: fileName];
    fName = [fName stringByAppendingString:fPath];
    dir = [dir stringByAppendingString:fName];
    return [dir fileSystemRepresentation];
}

static id<MTLSamplerState> GetSampler( teTextureSampler sampler )
{
    if (sampler == teTextureSampler::LinearClamp) return renderer.linearClamp;
    if (sampler == teTextureSampler::LinearRepeat) return renderer.linearRepeat;
    if (sampler == teTextureSampler::NearestClamp) return renderer.nearestClamp;
    if (sampler == teTextureSampler::NearestRepeat) return renderer.nearestRepeat;
    
    teAssert( !"Unhandled sampler!" );
    return renderer.linearClamp;
}

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

    MTLDepthStencilDescriptor* depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthCompareFunction = MTLCompareFunctionGreater;
    depthStateDesc.depthWriteEnabled = YES;
    depthStateDesc.label = @"greater write on";
    renderer.depthStateGreaterWriteOn = [renderer.device newDepthStencilStateWithDescriptor:depthStateDesc];
    
    depthStateDesc.depthCompareFunction = MTLCompareFunctionGreater;
    depthStateDesc.depthWriteEnabled = NO;
    depthStateDesc.label = @"greater write off";
    renderer.depthStateGreaterWriteOff = [renderer.device newDepthStencilStateWithDescriptor:depthStateDesc];
    
    depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
    depthStateDesc.depthWriteEnabled = NO;
    depthStateDesc.label = @"none write off";
    renderer.depthStateNoneWriteOff = [renderer.device newDepthStencilStateWithDescriptor:depthStateDesc];

    defaultLibrary = [renderer.device newDefaultLibrary];
    
    renderer.width = width;
    renderer.height = height;
    
    renderer.commandQueue = [renderer.device newCommandQueue];
    gCommandQueue = renderer.commandQueue;
    renderer.renderPassDescriptorFBO = [MTLRenderPassDescriptor renderPassDescriptor];
    
    const unsigned bufferBytes = 1024 * 1024 * 250;
    
    renderer.staticMeshIndexBuffer = CreateBuffer( renderer.device, bufferBytes, false );
    renderer.staticMeshIndexStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true );
    renderer.staticMeshPositionBuffer = CreateBuffer( renderer.device, bufferBytes, false );
    renderer.staticMeshPositionStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true );
    renderer.staticMeshUVBuffer = CreateBuffer( renderer.device, bufferBytes, false );
    renderer.staticMeshUVStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true );
    
    const unsigned uiBufferBytes = 1024 * 1024 * 8;
    
    renderer.uiVertexBuffer = CreateBuffer( renderer.device, uiBufferBytes, true );
    renderer.uiIndexBuffer = CreateBuffer( renderer.device, uiBufferBytes, true );
    renderer.uiVertices = (float*)teMalloc( 1024 * 1024 * 8 );
    renderer.uiIndices = (uint16_t*)teMalloc( 1024 * 1024 * 8 );

    for (unsigned i = 0; i < 2; ++i)
    {
        constexpr unsigned UniformBufferSize = sizeof( PerObjectUboStruct ) * 10000;

#if !TARGET_OS_IPHONE
        renderer.frameResources[ i ].uniformBuffer = [renderer.device newBufferWithLength:UniformBufferSize options:MTLResourceStorageModeManaged];
#else
        renderer.frameResources[ i ].uniformBuffer = [renderer.device newBufferWithLength:UniformBufferSize options:MTLResourceCPUCacheModeDefaultCache];
#endif
        renderer.frameResources[ i ].uniformBuffer.label = @"uniform buffer";
    }
    
    renderer.defaultTexture2D = teCreateTexture2D( 32, 32, 0, teTextureFormat::RGBA_sRGB, "default texture 2D" );
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

    teMemcpy( bufferPointer + offset, data, dataBytesNextMultipleOf4 );
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
    renderer.frameResources[ 0 ].commandBuffer = [renderer.commandQueue commandBuffer];
    renderer.frameResources[ 0 ].commandBuffer.label = @"MyCommand";
    gCommandBuffer = renderer.frameResources[ 0 ].commandBuffer;

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

void BeginRendering( teTexture2D& color, teTexture2D& depth, teClearFlag clearFlag, const float* clearColor )
{
    if (color.index != -1 && depth.index != -1)
    {
        renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].clearColor = MTLClearColorMake( clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] );
        renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].loadAction = clearFlag == teClearFlag::DepthAndColor ? MTLLoadActionClear : MTLLoadActionLoad;
        renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].texture = TextureGetMetalTexture( color.index );
        renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].resolveTexture = nil;
        renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].storeAction = MTLStoreActionStore;
        renderer.renderPassDescriptorFBO.depthAttachment.loadAction = clearFlag != teClearFlag::DontClear ? MTLLoadActionClear : MTLLoadActionLoad;
        renderer.renderPassDescriptorFBO.depthAttachment.clearDepth = 0;
        renderer.renderPassDescriptorFBO.depthAttachment.texture = TextureGetMetalTexture( depth.index );
        
        renderer.renderEncoder = [renderer.frameResources[ 0 ].commandBuffer renderCommandEncoderWithDescriptor:renderer.renderPassDescriptorFBO];
        renderer.renderEncoder.label = @"RenderEncoderFBO";
    }
    else if (color.index == -1 && depth.index == -1)
    {
        renderPassDescriptor.colorAttachments[ 0 ].clearColor = MTLClearColorMake( clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] );
        renderPassDescriptor.colorAttachments[ 0 ].loadAction = clearFlag == teClearFlag::DepthAndColor ? MTLLoadActionClear : MTLLoadActionLoad;
        renderPassDescriptor.colorAttachments[ 0 ].resolveTexture = nil;
        renderPassDescriptor.colorAttachments[ 0 ].storeAction = MTLStoreActionStore;
        renderPassDescriptor.depthAttachment.loadAction = clearFlag != teClearFlag::DontClear ? MTLLoadActionClear : MTLLoadActionLoad;
        renderPassDescriptor.depthAttachment.clearDepth = 0;
        
        renderer.renderEncoder = [renderer.frameResources[ 0 ].commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderer.renderEncoder.label = @"RenderEncoderFBO";
    }
}

void EndRendering()
{
    [renderer.renderEncoder endEncoding];
}

void teBeginSwapchainRendering( teTexture2D& color )
{
    teTexture2D nullColor, nullDepth;
    nullColor.index = -1;
    nullDepth.index = -1;
    float clearColor[ 4 ] = { 0, 0, 0, 1 };
    BeginRendering( nullColor, nullDepth, teClearFlag::DepthAndColor, clearColor );
}

void teEndSwapchainRendering()
{
    [renderer.renderEncoder endEncoding];
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

static int GetPSO( id<MTLFunction> vertexProgram, id<MTLFunction> pixelProgram, teBlendMode blendMode, teTopology topology, MTLPixelFormat format, bool isUI )
{
    int psoIndex = -1;
    
    for (unsigned i = 0; i < MaxPSOs; ++i)
    {
        if (renderer.psos[ i ].blendMode == blendMode && renderer.psos[ i ].topology == topology &&
            renderer.psos[ i ].vertexFunction == vertexProgram && renderer.psos[ i ].pixelFunction == pixelProgram &&
            renderer.psos[ i ].format == format )
        {
            return i;
        }
    }
    
    if (psoIndex == -1)
    {
        MTLRenderPipelineDescriptor* pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.rasterSampleCount = 1;
        pipelineStateDescriptor.vertexFunction = vertexProgram;
        pipelineStateDescriptor.fragmentFunction = pixelProgram;
#if !TARGET_OS_IPHONE
        pipelineStateDescriptor.inputPrimitiveTopology = topology == teTopology::Triangles ? MTLPrimitiveTopologyClassTriangle : MTLPrimitiveTopologyClassLine;
#endif
        pipelineStateDescriptor.colorAttachments[ 0 ].pixelFormat = format;
        pipelineStateDescriptor.colorAttachments[ 0 ].blendingEnabled = blendMode != teBlendMode::Off;
        pipelineStateDescriptor.colorAttachments[ 0 ].sourceRGBBlendFactor = blendMode == teBlendMode::Alpha ? MTLBlendFactorSourceAlpha : MTLBlendFactorOne;
        pipelineStateDescriptor.colorAttachments[ 0 ].destinationRGBBlendFactor = blendMode == teBlendMode::Alpha ?  MTLBlendFactorOneMinusSourceAlpha : MTLBlendFactorOne;
        pipelineStateDescriptor.colorAttachments[ 0 ].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[ 0 ].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[ 0 ].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[ 0 ].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        
        if (isUI)
        {
            MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor vertexDescriptor];
            
            // Position
            vertexDesc.attributes[ 0 ].format = MTLVertexFormatFloat2;
            vertexDesc.attributes[ 0 ].bufferIndex = 0;
            vertexDesc.attributes[ 0 ].offset = 0;
            
            // Texcoord
            vertexDesc.attributes[ 1 ].format = MTLVertexFormatFloat2;
            vertexDesc.attributes[ 1 ].bufferIndex = 0;
            vertexDesc.attributes[ 1 ].offset = 4 * 2;

            // Color
            vertexDesc.attributes[ 2 ].format = MTLVertexFormatUInt;
            vertexDesc.attributes[ 2 ].bufferIndex = 0;
            vertexDesc.attributes[ 2 ].offset = 4 * 4;

            vertexDesc.layouts[ 0 ].stride = 4 * 4 + 4; // sizeof( ImDrawVert )
            vertexDesc.layouts[ 0 ].stepFunction = MTLVertexStepFunctionPerVertex;
            
            pipelineStateDescriptor.vertexDescriptor = vertexDesc;
        }
        
        NSError* error = nullptr;
        MTLPipelineOption option = MTLPipelineOptionNone;
        
        int nextFreePsoIndex = -1;
        
        for (unsigned i = 0; i < MaxPSOs && nextFreePsoIndex == -1; ++i)
        {
            if (nextFreePsoIndex == -1 && renderer.psos[ i ].pso == nil)
            {
                nextFreePsoIndex = i;
            }
        }
        
        teAssert( nextFreePsoIndex != -1 );

        renderer.psos[ nextFreePsoIndex ].pso = [renderer.device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor options:option reflection:nullptr error:&error];
        
        if (!renderer.psos[ nextFreePsoIndex ].pso)
        {
            NSLog( @"Failed to create pipeline state, error %@", error );
        }
        
        psoIndex = nextFreePsoIndex;
        renderer.psos[ psoIndex ].blendMode = blendMode;
        renderer.psos[ psoIndex ].vertexFunction = vertexProgram;
        renderer.psos[ psoIndex ].pixelFunction = pixelProgram;
        renderer.psos[ psoIndex ].topology = topology;
        renderer.psos[ psoIndex ].format = format;
    }

    return psoIndex;
}

void Draw( const teShader& shader, unsigned positionOffset, unsigned uvOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, teTextureFormat colorFormat, teTextureFormat depthFormat, unsigned textureIndex,
          teTextureSampler sampler )
{
    id< MTLTexture > textures[] = { TextureGetMetalTexture( textureIndex ), TextureGetMetalTexture( textureIndex ), TextureGetMetalTexture( textureIndex ) };
    NSRange range = { 0, 3 };
    [renderer.renderEncoder setFragmentTextures:textures withRange:range ];
    
    [renderer.renderEncoder setFragmentSamplerState:GetSampler( sampler ) atIndex:0];

    MTLPixelFormat format = renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].texture.pixelFormat;
    const int psoIndex = GetPSO( teShaderGetVertexProgram( shader ), teShaderGetPixelProgram( shader ), blendMode, topology, format, false );

    [renderer.renderEncoder setRenderPipelineState:renderer.psos[ psoIndex ].pso];
    [renderer.renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [renderer.renderEncoder setCullMode:(MTLCullMode)cullMode];

    // Note that we're using reverse-z, so less equal is really greater.
    if (depthMode == teDepthMode::LessOrEqualWriteOn)
    {
        [renderer.renderEncoder setDepthStencilState:renderer.depthStateGreaterWriteOn];
    }
    if (depthMode == teDepthMode::LessOrEqualWriteOff)
    {
        [renderer.renderEncoder setDepthStencilState:renderer.depthStateGreaterWriteOff];
    }
    else if (depthMode == teDepthMode::NoneWriteOff)
    {
        [renderer.renderEncoder setDepthStencilState:renderer.depthStateNoneWriteOff];
    }

    id< MTLBuffer > buffers[] = { renderer.frameResources[ 0 ].uniformBuffer, BufferGetBuffer( renderer.staticMeshPositionBuffer ), BufferGetBuffer( renderer.staticMeshUVBuffer ) };
    NSUInteger offsets[] = { renderer.frameResources[ 0 ].uboOffset, positionOffset, uvOffset };
    
    [renderer.renderEncoder setTriangleFillMode:fillMode == teFillMode::Solid ? MTLTriangleFillModeFill : MTLTriangleFillModeLines];
    [renderer.renderEncoder setFragmentBuffer:renderer.frameResources[ 0 ].uniformBuffer offset:renderer.frameResources[ 0 ].uboOffset atIndex:0];
    [renderer.renderEncoder setVertexBuffers:buffers offsets:offsets withRange:range];
    [renderer.renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                              indexCount:indexCount * 3
                               indexType:MTLIndexTypeUInt16
                             indexBuffer:BufferGetBuffer( renderer.staticMeshIndexBuffer )
                       indexBufferOffset:indexOffset];

    renderer.frameResources[ 0 ].uboOffset += sizeof( PerObjectUboStruct );
}

void teDrawFullscreenTriangle( teShader& shader, teTexture2D& texture )
{
    Draw( shader, 0, 0, 3, 0, teBlendMode::Off, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, renderer.colorFormat, teTextureFormat::Depth32F, texture.index, teTextureSampler::NearestClamp );
}

void teMapUiMemory( void** outVertexMemory, void** outIndexMemory )
{
    *outVertexMemory = [BufferGetBuffer( renderer.uiVertexBuffer ) contents];
    *outIndexMemory = [BufferGetBuffer( renderer.uiIndexBuffer ) contents];
}

void teUnmapUiMemory()
{
}

void teUIDrawCall( const teShader& shader, const teTexture2D& fontTex, int displaySizeX, int displaySizeY, int scissorX, int scissorY, unsigned scissorW, unsigned scissorH, unsigned elementCount, unsigned indexOffset, unsigned vertexOffset )
{
    MTLPixelFormat format = renderer.renderPassDescriptorFBO.colorAttachments[ 0 ].texture.pixelFormat;
    const int psoIndex = GetPSO( teShaderGetVertexProgram( shader ), teShaderGetPixelProgram( shader ), teBlendMode::Alpha, teTopology::Triangles, format, true );
    
    MTLScissorRect scissor;
    scissor.x = scissorX;
    scissor.y = scissorY;
    scissor.width = scissorW;
    scissor.height = scissorH;
    
    float displayPosX = 0;
    float displayPosY = 0;
    float scale[ 2 ];
    float translate[ 2 ];
    scale[ 0 ] = 2.0f / displaySizeX;
    scale[ 1 ] = 2.0f / displaySizeY;
    translate[ 0 ] = -1.0f - displayPosX * scale[ 0 ];
    translate[ 1 ] = -1.0f - displayPosY * scale[ 1 ];

    Matrix parameters; // Reusing UBO localToClip to store UI scale and translate.
    parameters.m[ 0 ] = scale[ 0 ];
    parameters.m[ 1 ] = scale[ 1 ];
    parameters.m[ 2 ] = translate[ 0 ];
    parameters.m[ 3 ] = translate[ 1 ];
    UpdateUBO( parameters.m, parameters.m, parameters.m, parameters.m );

    [renderer.renderEncoder setRenderPipelineState:renderer.psos[ psoIndex ].pso];
    [renderer.renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [renderer.renderEncoder setCullMode:MTLCullModeNone];
    [renderer.renderEncoder setScissorRect:scissor];
    [renderer.renderEncoder setDepthStencilState:renderer.depthStateNoneWriteOff];
    [renderer.renderEncoder setTriangleFillMode:MTLTriangleFillModeFill];
    [renderer.renderEncoder setVertexBuffer:BufferGetBuffer( renderer.uiVertexBuffer ) offset:vertexOffset atIndex:0];
    [renderer.renderEncoder setVertexBuffer:renderer.frameResources[ 0 ].uniformBuffer offset:renderer.frameResources[ 0 ].uboOffset atIndex:1];
    [renderer.renderEncoder setFragmentTexture:TextureGetMetalTexture( fontTex.index ) atIndex:0];
    [renderer.renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                              indexCount:elementCount
                               indexType:MTLIndexTypeUInt16
                             indexBuffer:BufferGetBuffer( renderer.uiIndexBuffer )
                       indexBufferOffset:indexOffset];
    
    renderer.frameResources[ 0 ].uboOffset += sizeof( PerObjectUboStruct );
}
