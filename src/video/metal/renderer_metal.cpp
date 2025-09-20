//#define NS_PRIVATE_IMPLEMENTATION
//#define CA_PRIVATE_IMPLEMENTATION
//#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include "buffer.h"
#include "camera.h"
#include "material.h"
#include "matrix.h"
#include "mesh.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "te_stdlib.h"
#include "vec3.h"

void InitLightTiler( unsigned widthPixels, unsigned heightPixels );
teBuffer CreateBuffer( MTL::Device* device, unsigned dataBytes, bool isStaging, const char* debugName );
unsigned BufferGetSizeBytes( const teBuffer& buffer );
MTL::Buffer* BufferGetBuffer( const teBuffer& buffer );
MTL::Function* teShaderGetVertexProgram( const teShader& shader );
MTL::Function* teShaderGetPixelProgram( const teShader& shader );
MTL::Texture* TextureGetMetalTexture( unsigned index );
unsigned GetMaxLightsPerTile( unsigned height );
unsigned GetPointLightCount();
unsigned GetSpotLightCount();
teBuffer GetPointLightCenterAndRadiusBuffer();
teBuffer GetPointLightColorBuffer();

static const unsigned MaxPSOs = 100;

// Must match shader header ubo.h
struct PerObjectUboStruct
{
    Matrix   localToClip;
    Matrix   localToView;
    Matrix   localToShadowClip;
    Matrix   localToWorld;
    Matrix   clipToView;
    Vec4     bloomParams;
    Vec4     tilesXY;
    Vec4     tint{ 1, 1, 1, 1  };
    Vec4     lightDir;
    Vec4     lightColor;
    Vec4     lightPosition;
    unsigned pointLightCount{ 0 };
    unsigned spotLightCount{ 0 };
    unsigned maxLightsPerTile{ 0 };
};

constexpr unsigned UniformBufferSize = sizeof( PerObjectUboStruct ) * 10000;

struct FrameResource
{
    MTL::CommandBuffer* commandBuffer;
    MTL::Buffer*        uniformBuffer;
    unsigned            uboOffset = 0;
};

struct PSO
{
    MTL::RenderPipelineState* pso;
    MTL::Function*            vertexFunction;
    MTL::Function*            pixelFunction;
    MTL::PixelFormat          colorFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
    MTL::PixelFormat          depthFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
    teBlendMode               blendMode = teBlendMode::Off;
    teTopology                topology = teTopology::Triangles;
};

struct Renderer
{
    MTL::Device*               device;
    MTL::RenderCommandEncoder* renderEncoder;
    MTL::CommandQueue*         commandQueue;
    MTL::RenderPassDescriptor* renderPassDescriptorFBO;
    FrameResource              frameResources[ 2 ];
    teTextureFormat            colorFormat = teTextureFormat::BGRA_sRGB;
    PSO psos[ MaxPSOs ];
    
    MTL::DepthStencilState* depthStateGreaterWriteOn;
    MTL::DepthStencilState* depthStateGreaterWriteOff;
    MTL::DepthStencilState* depthStateNoneWriteOff;
    MTL::DepthStencilState* depthStateLessEqualWriteOn;
    MTL::DepthStencilState* depthStateLessEqualWriteOff;

    MTL::SamplerState* anisotropicRepeat;
    MTL::SamplerState* anisotropicClamp;
    MTL::SamplerState* linearRepeat;
    MTL::SamplerState* linearClamp;
    MTL::SamplerState* nearestRepeat;
    MTL::SamplerState* nearestClamp;

    unsigned indexCounter = 0;
    unsigned uvCounter = 0;
    unsigned positionCounter = 0;
    unsigned normalCounter = 0;
    unsigned tangentCounter = 0;

    unsigned width = 0;
    unsigned height = 0;
    
    teBuffer staticMeshIndexBuffer;
    teBuffer staticMeshIndexStagingBuffer;
    teBuffer staticMeshPositionBuffer;
    teBuffer staticMeshPositionStagingBuffer;
    teBuffer staticMeshUVStagingBuffer;
    teBuffer staticMeshUVBuffer;
    teBuffer staticMeshNormalBuffer;
    teBuffer staticMeshNormalStagingBuffer;
    teBuffer staticMeshTangentBuffer;
    teBuffer staticMeshTangentStagingBuffer;

    teBuffer    uiVertexBuffer;
    teBuffer    uiIndexBuffer;
    float*      uiVertices = nullptr;
    uint16_t*   uiIndices = nullptr;

    teTexture2D defaultTexture2D;
    
    unsigned statDrawCalls = 0;
    unsigned statPSOBinds = 0;
};

Renderer renderer;

MTL::Library* shaderLibrary; // Shaders from shaders.metallib, must be loaded with teLoadMetalShaderLibrary().
CA::MetalDrawable* gDrawable;
MTL::Device* gDevice;
MTL::CommandQueue* gCommandQueue;
MTL::RenderPassDescriptor* renderPassDescriptor; // This comes from the application
MTL::CommandBuffer* gCommandBuffer; // This is used by the application.

MTL::Buffer* GetUniformBufferAndOffset( unsigned& outOffset )
{
    outOffset = renderer.frameResources[ 0 ].uboOffset;
    
    return renderer.frameResources[ 0 ].uniformBuffer;
}

static MTL::SamplerState* GetSampler( teTextureSampler sampler )
{
    if (sampler == teTextureSampler::LinearClamp) return renderer.linearClamp;
    if (sampler == teTextureSampler::LinearRepeat) return renderer.linearRepeat;
    if (sampler == teTextureSampler::NearestClamp) return renderer.nearestClamp;
    if (sampler == teTextureSampler::NearestRepeat) return renderer.nearestRepeat;
    
    teAssert( !"Unhandled sampler!" );
    return renderer.linearClamp;
}

void RendererGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = renderer.width;
    outHeight = renderer.height;
}

void teLoadMetalShaderLibrary()
{
    NS::Error* error = nullptr;
    shaderLibrary = renderer.device->newLibrary( NS::String::string( "shaders.metallib", NS::UTF8StringEncoding ), &error );

    if (!shaderLibrary)
    {
        printf( "Failed to load (shaders.metallib)library. error ===== %s\n", error->localizedDescription()->utf8String() );
    }
}

teBuffer CreateBuffer( unsigned size, const char* debugName )
{
    return CreateBuffer( renderer.device, size, false, debugName );
}

teBuffer CreateStagingBuffer( unsigned size, const char* debugName )
{
    return CreateBuffer( renderer.device, size, true, debugName );
}

void teCreateRenderer( unsigned swapInterval, void* windowHandle, unsigned width, unsigned height )
{
    renderer.device = MTL::CreateSystemDefaultDevice();
    gDevice = renderer.device;
    
    MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();

    samplerDescriptor->setMinFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMagFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMipFilter( MTL::SamplerMipFilterLinear );
    samplerDescriptor->setSAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setTAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setRAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setMaxAnisotropy( 1 );
    //samplerDescriptor.label = @"linear repeat";
    renderer.linearRepeat = renderer.device->newSamplerState( samplerDescriptor );

    samplerDescriptor->setMinFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMagFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMipFilter( MTL::SamplerMipFilterLinear );
    samplerDescriptor->setSAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setTAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setRAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setMaxAnisotropy( 1 );
    samplerDescriptor->setLabel( NS::String::string( "linearClamp", NS::UTF8StringEncoding ) );
    renderer.linearClamp = renderer.device->newSamplerState( samplerDescriptor );

    samplerDescriptor->setMinFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMagFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMipFilter( MTL::SamplerMipFilterLinear );
    samplerDescriptor->setSAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setTAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setRAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setMaxAnisotropy( 8 );
    samplerDescriptor->setLabel( NS::String::string( "anisotropicRepeat", NS::UTF8StringEncoding ) );
    renderer.anisotropicRepeat = renderer.device->newSamplerState( samplerDescriptor );

    samplerDescriptor->setMinFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMagFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMipFilter( MTL::SamplerMipFilterLinear );
    samplerDescriptor->setSAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setTAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setRAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setMaxAnisotropy( 8 );
    samplerDescriptor->setLabel( NS::String::string( "anisotropicClamp", NS::UTF8StringEncoding ) );
    renderer.anisotropicClamp = renderer.device->newSamplerState( samplerDescriptor );

    samplerDescriptor->setMinFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMagFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMipFilter( MTL::SamplerMipFilterNearest );
    samplerDescriptor->setSAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setTAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setRAddressMode( MTL::SamplerAddressModeRepeat );
    samplerDescriptor->setMaxAnisotropy( 1 );
    samplerDescriptor->setLabel( NS::String::string( "nearestRepeat", NS::UTF8StringEncoding ) );
    renderer.nearestRepeat = renderer.device->newSamplerState( samplerDescriptor );

    samplerDescriptor->setMinFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMagFilter( MTL::SamplerMinMagFilterLinear );
    samplerDescriptor->setMipFilter( MTL::SamplerMipFilterNearest );
    samplerDescriptor->setSAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setTAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setRAddressMode( MTL::SamplerAddressModeClampToEdge );
    samplerDescriptor->setMaxAnisotropy( 1 );
    samplerDescriptor->setLabel( NS::String::string( "nearestClamp", NS::UTF8StringEncoding ) );
    renderer.nearestClamp = renderer.device->newSamplerState( samplerDescriptor );

    MTL::DepthStencilDescriptor* depthStateDesc = MTL::DepthStencilDescriptor::alloc()->init();
    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionGreater );
    depthStateDesc->setDepthWriteEnabled( true );
    depthStateDesc->setLabel( NS::String::string( "depthStateGreaterWriteOn", NS::UTF8StringEncoding ) );
    renderer.depthStateGreaterWriteOn = renderer.device->newDepthStencilState( depthStateDesc );
    
    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionGreater );
    depthStateDesc->setDepthWriteEnabled( false );
    depthStateDesc->setLabel( NS::String::string( "depthStateGreaterWriteOff", NS::UTF8StringEncoding ) );
    renderer.depthStateGreaterWriteOff = renderer.device->newDepthStencilState( depthStateDesc );
    
    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionAlways );
    depthStateDesc->setDepthWriteEnabled( false );
    depthStateDesc->setLabel( NS::String::string( "depthStateNoneWriteOff", NS::UTF8StringEncoding ) );
    renderer.depthStateNoneWriteOff = renderer.device->newDepthStencilState( depthStateDesc );

    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionLessEqual );
    depthStateDesc->setDepthWriteEnabled( false );
    depthStateDesc->setLabel( NS::String::string( "depthStateLessEqualWriteOff", NS::UTF8StringEncoding ) );
    renderer.depthStateLessEqualWriteOff = renderer.device->newDepthStencilState( depthStateDesc );

    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionLessEqual );
    depthStateDesc->setDepthWriteEnabled( true );
    depthStateDesc->setLabel( NS::String::string( "depthStateLessEqualWriteOn", NS::UTF8StringEncoding ) );
    renderer.depthStateLessEqualWriteOn = renderer.device->newDepthStencilState( depthStateDesc );

    renderer.width = width;
    renderer.height = height;
    
    renderer.commandQueue = renderer.device->newCommandQueue();
    gCommandQueue = renderer.commandQueue;
    renderer.renderPassDescriptorFBO = MTL::RenderPassDescriptor::alloc()->init();
    
    const unsigned bufferBytes = 1024 * 1024 * 250;
    
    renderer.staticMeshIndexBuffer = CreateBuffer( renderer.device, bufferBytes, false, "staticMeshIndexBuffer" );
    renderer.staticMeshIndexStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true, "staticMeshIndexStagingBuffer" );
    renderer.staticMeshPositionBuffer = CreateBuffer( renderer.device, bufferBytes, false, "staticMeshPositionBuffer" );
    renderer.staticMeshPositionStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true, "staticMeshPositionStagingBuffer" );
    renderer.staticMeshUVBuffer = CreateBuffer( renderer.device, bufferBytes, false, "staticMeshUVBuffer" );
    renderer.staticMeshUVStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true, "staticMeshUVStagingBuffer" );
    renderer.staticMeshNormalBuffer = CreateBuffer( renderer.device, bufferBytes, false, "staticMeshNormalBuffer" );
    renderer.staticMeshNormalStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true, "staticMeshNormalStagingBuffer" );
    renderer.staticMeshTangentBuffer = CreateBuffer( renderer.device, bufferBytes, false, "staticMeshTangentBuffer" );
    renderer.staticMeshTangentStagingBuffer = CreateBuffer( renderer.device, bufferBytes, true, "staticMeshTangentStagingBuffer" );

    const unsigned uiBufferBytes = 1024 * 1024 * 8;
    
    renderer.uiVertexBuffer = CreateBuffer( renderer.device, uiBufferBytes, true, "uiVertexBuffer" );
    renderer.uiIndexBuffer = CreateBuffer( renderer.device, uiBufferBytes, true, "uiIndexBuffer" );
    renderer.uiVertices = (float*)teMalloc( 1024 * 1024 * 8 );
    renderer.uiIndices = (uint16_t*)teMalloc( 1024 * 1024 * 8 );

    for (unsigned i = 0; i < 2; ++i)
    {
#if !TARGET_OS_IPHONE
        renderer.frameResources[ i ].uniformBuffer = renderer.device->newBuffer( UniformBufferSize, MTL::ResourceStorageModeManaged );
#else
        renderer.frameResources[ i ].uniformBuffer = renderer.device->newBuffer( UniformBufferSize, MTL::ResourceCPUCacheModeDefaultCache );
#endif
        renderer.frameResources[ i ].uniformBuffer->setLabel( NS::String::string( "uniform buffer", NS::UTF8StringEncoding ) );
    }
    
    renderer.defaultTexture2D = teCreateTexture2D( 32, 32, 0, teTextureFormat::RGBA_sRGB, "default texture 2D" );

    InitLightTiler( width, height );
}

void PushGroupMarker( const char* name )
{
    //NSString* fName = [NSString stringWithUTF8String: name];
    //[renderer.renderEncoder pushDebugGroup:fName];
    renderer.renderEncoder->pushDebugGroup( NS::String::string( name, NS::UTF8StringEncoding ) );
}

void PopGroupMarker()
{
    //[renderer.renderEncoder popDebugGroup];
    renderer.renderEncoder->popDebugGroup();
}

void UpdateStagingBuffer( const teBuffer& buffer, const void* data, unsigned dataBytes, unsigned offset )
{
    const unsigned dataBytesNextMultipleOf4 = ((dataBytes + 3) / 4) * 4;
    
    uint8_t* bufferPointer = (uint8_t *)(BufferGetBuffer( buffer )->contents());

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

unsigned AddNormals( const float* normals, unsigned bytes )
{
    if (normals)
    {
        UpdateStagingBuffer( renderer.staticMeshNormalStagingBuffer, normals, bytes, renderer.normalCounter );
    }

    renderer.normalCounter += bytes;
    return renderer.normalCounter - bytes;
}

unsigned AddTangents( const float* tangents, unsigned bytes )
{
    if (tangents)
    {
        UpdateStagingBuffer( renderer.staticMeshTangentStagingBuffer, tangents, bytes, renderer.tangentCounter );
    }

    renderer.tangentCounter += bytes;
    return renderer.tangentCounter - bytes;
}

void UpdateUBO( const float localToClip[ 16 ], const float localToShadowClip[ 16 ],
                const float localToWorld[ 16 ],
                const ShaderParams& shaderParams, const Vec4& lightDir, const Vec4& lightColor, const Vec4& lightPosition )
{
    PerObjectUboStruct uboStruct = {};
    uboStruct.localToClip.InitFrom( localToClip );
    uboStruct.localToView.InitFrom( shaderParams.localToView );
    uboStruct.localToShadowClip.InitFrom( localToShadowClip );
    uboStruct.localToWorld.InitFrom( localToWorld );
    uboStruct.clipToView.InitFrom( shaderParams.clipToView );
    uboStruct.bloomParams.w = shaderParams.bloomThreshold;
    uboStruct.tilesXY.x = shaderParams.tilesXY[ 0 ];
    uboStruct.tilesXY.y = shaderParams.tilesXY[ 1 ];
    uboStruct.tilesXY.w = shaderParams.tilesXY[ 2 ];
    uboStruct.tilesXY.z = shaderParams.tilesXY[ 3 ];
    uboStruct.lightDir = lightDir;
    uboStruct.lightColor = lightColor;
    uboStruct.lightPosition = lightPosition;
    uboStruct.tint.x = shaderParams.tint[ 0 ];
    uboStruct.tint.y = shaderParams.tint[ 1 ];
    uboStruct.tint.z = shaderParams.tint[ 2 ];
    uboStruct.tint.w = shaderParams.tint[ 3 ];
    uboStruct.maxLightsPerTile = GetMaxLightsPerTile( renderer.height );
    uboStruct.pointLightCount = GetPointLightCount();
    uboStruct.spotLightCount = GetSpotLightCount();

    MTL::Buffer* uniformBuffer = renderer.frameResources[ 0 ].uniformBuffer;
    uint8_t* bufferPointer = (uint8_t*)(uniformBuffer->contents()) + renderer.frameResources[ 0 ].uboOffset;

    memcpy( bufferPointer, &uboStruct, sizeof( PerObjectUboStruct ) );
#if !TARGET_OS_IPHONE
    uniformBuffer->didModifyRange( NS::Range::Make( renderer.frameResources[ 0 ].uboOffset, sizeof( PerObjectUboStruct ) ) );
#endif
}

void teBeginFrame()
{
    renderer.frameResources[ 0 ].commandBuffer = renderer.commandQueue->commandBuffer();
    renderer.frameResources[ 0 ].commandBuffer->setLabel( NS::String::string( "command buffer", NS::UTF8StringEncoding ) );
    renderer.frameResources[ 0 ].uboOffset = 0;
    gCommandBuffer = renderer.frameResources[ 0 ].commandBuffer;

    renderer.statDrawCalls = 0;
    renderer.statPSOBinds = 0;
}

teTextureFormat GetSwapchainColorFormat()
{
    return renderer.colorFormat;
}

void SetDrawable( CA::MetalDrawable* drawable )
{
    gDrawable = drawable;
}

void teEndFrame()
{
    renderer.frameResources[ 0 ].commandBuffer->presentDrawable( gDrawable );
    renderer.frameResources[ 0 ].commandBuffer->commit();
}

void BeginRendering( teTexture2D& color, teTexture2D& depth, teClearFlag clearFlag, const float* clearColor )
{
    if (color.index != -1 && depth.index != -1)
    {
        renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->setClearColor( MTL::ClearColor::Make( clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] ) );
        renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->setLoadAction( clearFlag == teClearFlag::DepthAndColor ? MTL::LoadActionClear : MTL::LoadActionLoad );
        renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->setTexture( TextureGetMetalTexture( color.index ) );
        renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->setResolveTexture( nullptr );
        renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->setStoreAction( MTL::StoreActionStore );
        renderer.renderPassDescriptorFBO->depthAttachment()->setLoadAction( clearFlag != teClearFlag::DontClear ? MTL::LoadActionClear : MTL::LoadActionLoad );
        renderer.renderPassDescriptorFBO->depthAttachment()->setClearDepth( 1 );
        renderer.renderPassDescriptorFBO->depthAttachment()->setTexture( TextureGetMetalTexture( depth.index ) );
        
        renderer.renderEncoder = renderer.frameResources[ 0 ].commandBuffer->renderCommandEncoder( renderer.renderPassDescriptorFBO );
        renderer.renderEncoder->setLabel( NS::String::string( "encoder fbo", NS::UTF8StringEncoding ) );
    }
    else if (color.index == -1 && depth.index == -1)
    {
        renderPassDescriptor->colorAttachments()->object( 0 )->setClearColor( MTL::ClearColor::Make( clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] ) );
        renderPassDescriptor->colorAttachments()->object( 0 )->setLoadAction( clearFlag == teClearFlag::DepthAndColor ? MTL::LoadActionClear : MTL::LoadActionLoad );
        renderPassDescriptor->colorAttachments()->object( 0 )->setResolveTexture( nullptr );
        renderPassDescriptor->colorAttachments()->object( 0 )->setStoreAction( MTL::StoreActionStore );
        renderPassDescriptor->depthAttachment()->setLoadAction( clearFlag != teClearFlag::DontClear ? MTL::LoadActionClear : MTL::LoadActionLoad );
        renderPassDescriptor->depthAttachment()->setClearDepth( 1 );
        
        renderer.renderEncoder = renderer.frameResources[ 0 ].commandBuffer->renderCommandEncoder( renderPassDescriptor );
        renderer.renderEncoder->setLabel( NS::String::string( "encoder", NS::UTF8StringEncoding ) );
    }
}

void EndRendering( teTexture2D& color )
{
    renderer.renderEncoder->endEncoding();
}

void teBeginSwapchainRendering()
{
    teTexture2D nullColor, nullDepth;
    nullColor.index = -1;
    nullDepth.index = -1;
    float clearColor[ 4 ] = { 0, 0, 0, 1 };
    BeginRendering( nullColor, nullDepth, teClearFlag::DepthAndColor, clearColor );
}

void teEndSwapchainRendering()
{
    renderer.renderEncoder->endEncoding();
}

void CopyBuffer( const teBuffer& source, const teBuffer& destination )
{
    teAssert( BufferGetSizeBytes( source ) <= BufferGetSizeBytes( destination ) );
    
    MTL::CommandBuffer* cmd_buffer =  renderer.commandQueue->commandBuffer();
    cmd_buffer->setLabel( NS::String::string( "blit cmdbuffer", NS::UTF8StringEncoding ) );
    MTL::BlitCommandEncoder* blit_encoder = cmd_buffer->blitCommandEncoder();
    blit_encoder->copyFromBuffer( BufferGetBuffer( source ), 0, BufferGetBuffer( destination ), 0, BufferGetSizeBytes( source ) );
    blit_encoder->endEncoding();
    cmd_buffer->commit();
    cmd_buffer->waitUntilCompleted();
}

void teFinalizeMeshBuffers()
{
    CopyBuffer( renderer.staticMeshIndexStagingBuffer, renderer.staticMeshIndexBuffer );
    CopyBuffer( renderer.staticMeshUVStagingBuffer, renderer.staticMeshUVBuffer );
    CopyBuffer( renderer.staticMeshPositionStagingBuffer, renderer.staticMeshPositionBuffer );
    CopyBuffer( renderer.staticMeshNormalStagingBuffer, renderer.staticMeshNormalBuffer );
    CopyBuffer( renderer.staticMeshTangentStagingBuffer, renderer.staticMeshTangentBuffer );
}

static int GetPSO( MTL::Function* vertexProgram, MTL::Function* pixelProgram, teBlendMode blendMode, teTopology topology, MTL::PixelFormat colorFormat, MTL::PixelFormat depthFormat, bool isUI )
{
    for (unsigned i = 0; i < MaxPSOs; ++i)
    {
        if (renderer.psos[ i ].blendMode == blendMode && renderer.psos[ i ].topology == topology &&
            renderer.psos[ i ].vertexFunction == vertexProgram && renderer.psos[ i ].pixelFunction == pixelProgram &&
            renderer.psos[ i ].colorFormat == colorFormat &&
            renderer.psos[ i ].depthFormat == depthFormat)
        {
            return i;
        }
    }
    
    MTL::RenderPipelineDescriptor* pipelineStateDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineStateDescriptor->setRasterSampleCount( 1 );
    pipelineStateDescriptor->setVertexFunction( vertexProgram );
    pipelineStateDescriptor->setFragmentFunction( pixelProgram );
#if !TARGET_OS_IPHONE
    pipelineStateDescriptor->setInputPrimitiveTopology( topology == teTopology::Triangles ? MTL::PrimitiveTopologyClassTriangle : MTL::PrimitiveTopologyClassLine );
#endif
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setPixelFormat( colorFormat );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setBlendingEnabled( blendMode != teBlendMode::Off );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setSourceRGBBlendFactor( blendMode == teBlendMode::Alpha ? MTL::BlendFactorSourceAlpha : MTL::BlendFactorOne );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setDestinationRGBBlendFactor( blendMode == teBlendMode::Alpha ? MTL::BlendFactorOneMinusSourceAlpha : MTL::BlendFactorOne );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setRgbBlendOperation( MTL::BlendOperationAdd );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setSourceAlphaBlendFactor( MTL::BlendFactorOne );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setDestinationAlphaBlendFactor( MTL::BlendFactorOneMinusSourceAlpha );
    pipelineStateDescriptor->colorAttachments()->object( 0 )->setAlphaBlendOperation( MTL::BlendOperationAdd );
    pipelineStateDescriptor->setDepthAttachmentPixelFormat( depthFormat );
        
    if (isUI)
    {
        // FIXME: Should this use alloc()::init()?
        MTL::VertexDescriptor* vertexDesc = MTL::VertexDescriptor::vertexDescriptor();
            
        // Position
        vertexDesc->attributes()->object( 0 )->setFormat( MTL::VertexFormatFloat2 );
        vertexDesc->attributes()->object( 0 )->setBufferIndex( 0 );
        vertexDesc->attributes()->object( 0 )->setOffset( 0 );
            
        // Texcoord
        vertexDesc->attributes()->object( 1 )->setFormat( MTL::VertexFormatFloat2 );
        vertexDesc->attributes()->object( 1 )->setBufferIndex( 0 );
        vertexDesc->attributes()->object( 1 )->setOffset( 4 * 2 );

        // Color
        vertexDesc->attributes()->object( 2 )->setFormat( MTL::VertexFormatUChar4 );
        vertexDesc->attributes()->object( 2 )->setBufferIndex( 0 );
        vertexDesc->attributes()->object( 2 )->setOffset( 4 * 4 );

        vertexDesc->layouts()->object( 0 )->setStride( 4 * 4 + 4 ); // sizeof( ImDrawVert )
        vertexDesc->layouts()->object( 0 )->setStepFunction( MTL::VertexStepFunctionPerVertex );
        vertexDesc->layouts()->object( 0 )->setStepRate( 1 );
            
        pipelineStateDescriptor->setVertexDescriptor( vertexDesc );
    }
        
    NS::Error* error = nullptr;
    MTL::PipelineOption option = MTL::PipelineOptionNone;
        
    int nextFreePsoIndex = -1;
        
    for (unsigned i = 0; i < MaxPSOs && nextFreePsoIndex == -1; ++i)
    {
        if (nextFreePsoIndex == -1 && renderer.psos[ i ].pso == nil)
        {
            nextFreePsoIndex = i;
        }
    }
        
    teAssert( nextFreePsoIndex != -1 );

    renderer.psos[ nextFreePsoIndex ].pso = renderer.device->newRenderPipelineState( pipelineStateDescriptor, option, nullptr, &error );
        
    if (!renderer.psos[ nextFreePsoIndex ].pso)
    {
        printf( "Failed to create pipeline state, error %s\n", error->localizedDescription()->utf8String() );
    }
        
    unsigned psoIndex = nextFreePsoIndex;
    renderer.psos[ psoIndex ].blendMode = blendMode;
    renderer.psos[ psoIndex ].vertexFunction = vertexProgram;
    renderer.psos[ psoIndex ].pixelFunction = pixelProgram;
    renderer.psos[ psoIndex ].topology = topology;
    renderer.psos[ psoIndex ].colorFormat = colorFormat;
    renderer.psos[ psoIndex ].depthFormat = depthFormat;

    return psoIndex;
}

void MoveToNextUboOffset()
{
    renderer.frameResources[ 0 ].uboOffset += sizeof( PerObjectUboStruct );
    teAssert( renderer.frameResources[ 0 ].uboOffset < UniformBufferSize );
}

void Draw( const teShader& shader, unsigned positionOffset, unsigned uvOffset, unsigned normalOffset, unsigned tangentOffset, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode, unsigned textureIndex,
          teTextureSampler sampler, unsigned normalMapIndex, unsigned shadowMapIndex )
{
    MTL::Texture* textures[] = { TextureGetMetalTexture( textureIndex ), TextureGetMetalTexture( normalMapIndex ), TextureGetMetalTexture( shadowMapIndex ) };
    NS::Range rangeTextures = { 0, 3 };
    renderer.renderEncoder->setFragmentTextures( textures, rangeTextures );
    
    renderer.renderEncoder->setFragmentSamplerState( GetSampler( sampler ), 0 );

    MTL::PixelFormat colorFormat = renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->texture()->pixelFormat();
    MTL::PixelFormat depthFormat = renderer.renderPassDescriptorFBO->depthAttachment()->texture()->pixelFormat();
    const int psoIndex = GetPSO( teShaderGetVertexProgram( shader ), teShaderGetPixelProgram( shader ), blendMode, topology, colorFormat, depthFormat, false );

    renderer.renderEncoder->setRenderPipelineState( renderer.psos[ psoIndex ].pso );
    ++renderer.statPSOBinds;
    renderer.renderEncoder->setFrontFacingWinding( MTL::WindingCounterClockwise );
    renderer.renderEncoder->setCullMode( (MTL::CullMode)cullMode );

    if (depthMode == teDepthMode::LessOrEqualWriteOn)
    {
        renderer.renderEncoder->setDepthStencilState( renderer.depthStateLessEqualWriteOn );
    }
    else if (depthMode == teDepthMode::LessOrEqualWriteOff)
    {
        renderer.renderEncoder->setDepthStencilState( renderer.depthStateLessEqualWriteOff );
    }
    else if (depthMode == teDepthMode::NoneWriteOff)
    {
        renderer.renderEncoder->setDepthStencilState( renderer.depthStateNoneWriteOff );
    }

    NS::Range rangeOffsets = { 0, 5 };
    MTL::Buffer* buffers[] = { renderer.frameResources[ 0 ].uniformBuffer, BufferGetBuffer( renderer.staticMeshPositionBuffer ), BufferGetBuffer( renderer.staticMeshUVBuffer ), BufferGetBuffer( renderer.staticMeshNormalBuffer ), BufferGetBuffer( renderer.staticMeshTangentBuffer ) };
    NS::UInteger offsets[] = { renderer.frameResources[ 0 ].uboOffset, positionOffset, uvOffset, normalOffset, tangentOffset };
    
    renderer.renderEncoder->setTriangleFillMode( fillMode == teFillMode::Solid ? MTL::TriangleFillModeFill : MTL::TriangleFillModeLines );
    renderer.renderEncoder->setFragmentBuffer( renderer.frameResources[ 0 ].uniformBuffer, renderer.frameResources[ 0 ].uboOffset, 0 );
    renderer.renderEncoder->setFragmentBuffer( BufferGetBuffer( GetLightIndexBuffer() ), 0, 1 );
    renderer.renderEncoder->setFragmentBuffer( BufferGetBuffer( GetPointLightCenterAndRadiusBuffer() ), 0, 2 );
    renderer.renderEncoder->setFragmentBuffer( BufferGetBuffer( GetPointLightColorBuffer() ), 0, 3 );
    renderer.renderEncoder->setVertexBuffers( buffers, offsets, rangeOffsets );
    renderer.renderEncoder->drawIndexedPrimitives( MTL::PrimitiveTypeTriangle,
                              indexCount * 3,
                               MTL::IndexTypeUInt16,
                             BufferGetBuffer( renderer.staticMeshIndexBuffer ),
                       indexOffset);

    MoveToNextUboOffset();
    ++renderer.statDrawCalls;
}

void teDrawFullscreenTriangle( teShader& shader, teTexture2D& texture, const ShaderParams& shaderParams, teBlendMode blendMode )
{
    float m[ 16 ];
    UpdateUBO( m, m, m, shaderParams, Vec4( 0, 0, 0, 1 ), Vec4( 1, 1, 1, 1 ), Vec4( 1, 1, 1, 1 ) );

    Draw( shader, 0, 0, 0, 0, 3, 0, blendMode, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, texture.index, teTextureSampler::NearestClamp, 0, 0 );
}

void teMapUiMemory( void** outVertexMemory, void** outIndexMemory )
{
    *outVertexMemory = BufferGetBuffer( renderer.uiVertexBuffer )->contents();
    *outIndexMemory = BufferGetBuffer( renderer.uiIndexBuffer )->contents();
}

void teUnmapUiMemory()
{
}

float teRendererGetStat( teStat stat )
{
    if (stat == teStat::DrawCalls) return (float)renderer.statDrawCalls;
    else if (stat == teStat::PSOBinds) return (float)renderer.statPSOBinds;
    return 0;
}

void teUIDrawCall( const teShader& shader, const teTexture2D& fontTex, int displaySizeX, int displaySizeY, int scissorX, int scissorY, unsigned scissorW, unsigned scissorH, unsigned elementCount, unsigned indexOffset, unsigned vertexOffset )
{
    MTL::PixelFormat colorFormat = renderer.renderPassDescriptorFBO->colorAttachments()->object( 0 )->texture()->pixelFormat();
    MTL::PixelFormat depthFormat = renderer.renderPassDescriptorFBO->depthAttachment()->texture()->pixelFormat();
    const int psoIndex = GetPSO( teShaderGetVertexProgram( shader ), teShaderGetPixelProgram( shader ), teBlendMode::Alpha, teTopology::Triangles, colorFormat, depthFormat, true );
    
    MTL::ScissorRect scissor;
    scissor.x = scissorX;
    scissor.y = scissorY;
    scissor.width = scissorW;
    scissor.height = scissorH;
    
    float displayPosX = 0;
    float displayPosY = 0;
    float L = displayPosX;
    float R = displayPosX + displaySizeX;
    float T = displayPosY;
    float B = displayPosY + displaySizeY;
    float N = 0;
    float F = 1;
    const float orthoProjection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,   0.0f },
        { 0.0f,         0.0f,        1/(F-N),   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T), N/(F-N),   1.0f },
    };
    
    Matrix localToClip;
    localToClip.InitFrom( &orthoProjection[ 0 ][ 0 ] );
    ShaderParams shaderParams = {};
    UpdateUBO( localToClip.m, localToClip.m, localToClip.m, shaderParams, Vec4( 0, 0, 0, 1 ), Vec4( 1, 1, 1, 1 ), Vec4( 1, 1, 1, 1 ) );

    const unsigned vertexStride = 20; // sizeof( ImDrawVert )
    const unsigned indexStride = 2; // sizeof( ImDrawIdx )
    
    renderer.renderEncoder->setRenderPipelineState( renderer.psos[ psoIndex ].pso );
    ++renderer.statPSOBinds;
    renderer.renderEncoder->setFrontFacingWinding( MTL::WindingCounterClockwise );
    renderer.renderEncoder->setCullMode( MTL::CullModeNone );
    renderer.renderEncoder->setScissorRect( scissor );
    renderer.renderEncoder->setDepthStencilState( renderer.depthStateNoneWriteOff );
    renderer.renderEncoder->setTriangleFillMode( MTL::TriangleFillModeFill );
    renderer.renderEncoder->setVertexBuffer( BufferGetBuffer( renderer.uiVertexBuffer ), 0, 0 );
    renderer.renderEncoder->setVertexBufferOffset( vertexOffset * vertexStride, 0 );
    renderer.renderEncoder->setVertexBuffer( renderer.frameResources[ 0 ].uniformBuffer, renderer.frameResources[ 0 ].uboOffset, 1 );
    renderer.renderEncoder->setFragmentTexture( TextureGetMetalTexture( fontTex.index ), 0 );
    
    renderer.renderEncoder->drawIndexedPrimitives( MTL::PrimitiveTypeTriangle,
                              elementCount,
                               MTL::IndexTypeUInt16,
                             BufferGetBuffer( renderer.uiIndexBuffer ),
                       indexOffset * indexStride );
    
    MoveToNextUboOffset();
    ++renderer.statDrawCalls;
}
