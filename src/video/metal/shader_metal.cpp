#include <Metal/Metal.hpp>
#include "shader.h"
#include "texture.h"
#include "te_stdlib.h"
#include "vec3.h"

void UpdateUBO( const float localToClip[ 16 ], const float localToView[ 16 ], const float localToShadowClip[ 16 ], const float localToWorld[ 16 ], const ShaderParams& shaderParams, const Vec4& lightDir, const Vec4& lightColor, const Vec4& lightPosition );
void MoveToNextUboOffset();
unsigned TextureGetFlags( unsigned index );

extern MTL::Library* defaultLibrary;
extern MTL::Library* shaderLibrary;
extern MTL::CommandQueue* gCommandQueue;
extern MTL::Device* gDevice;

MTL::Texture* TextureGetMetalTexture( unsigned index );
MTL::Buffer* GetUniformBufferAndOffset( unsigned& outOffset );

struct ShaderImpl
{
    MTL::Function* vertexProgram;
    MTL::Function* pixelProgram;
    MTL::Function* computeProgram;
    MTL::ComputePipelineState* computePipeline;
    unsigned threadsPerThreadgroupX;
    unsigned threadsPerThreadgroupY;
    MTL::Size threadgroupCounts;
};

static constexpr unsigned MaxShaders = 40;
static ShaderImpl shaders[ MaxShaders ];
static unsigned shaderCount = 0;

MTL::Function* teShaderGetVertexProgram( const teShader& shader )
{
    return shaders[ shader.index ].vertexProgram;
}

MTL::Function* teShaderGetPixelProgram( const teShader& shader )
{
    return shaders[ shader.index ].pixelProgram;
}

teShader teCreateShader( const struct teFile& vertexFile, const teFile& pixelFile, const char* vertexName, const char* pixelName )
{
    teAssert( shaderCount < MaxShaders );

    teShader outShader;
    outShader.index = ++shaderCount;
    
    NS::String* vertexShaderName = NS::String::string( vertexName, NS::StringEncoding::UTF8StringEncoding );
    if (defaultLibrary != nullptr)
    {
        shaders[ outShader.index ].vertexProgram = defaultLibrary->newFunction( vertexShaderName );
    }

    if (shaders[ outShader.index ].vertexProgram == nullptr)
    {
        shaders[ outShader.index ].vertexProgram = shaderLibrary->newFunction( vertexShaderName );

        if (shaders[ outShader.index ].vertexProgram == nullptr)
        {
            //NSLog( @"Shader: Could not load %s!\n", vertexName );
            printf( "could not load vertex shader\n" );
            outShader.index = 0;
            return outShader;
        }
    }
    
    NS::String* pixelShaderName = NS::String::string( pixelName, NS::StringEncoding::UTF8StringEncoding );

    if (defaultLibrary != nullptr)
    {
        shaders[ outShader.index ].pixelProgram = defaultLibrary->newFunction( pixelShaderName );
    }

    if (shaders[ outShader.index ].pixelProgram == nullptr)
    {
        shaders[ outShader.index ].pixelProgram = shaderLibrary->newFunction( pixelShaderName );

        if (shaders[ outShader.index ].pixelProgram == nullptr)
        {
            //NSLog( @"Shader: Could not load %s!\n", pixelName );
            printf( "could not load pixel shader\n" );
            outShader.index = 0;
        }
    }

    return outShader;
}

teShader teCreateComputeShader( const teFile& file, const char* name, unsigned threadsPerThreadgroupX, unsigned threadsPerThreadgroupY )
{
    teAssert( shaderCount < MaxShaders );

    teShader outShader;
    outShader.index = ++shaderCount;
    
    NS::String* computeShaderName = NS::String::string( name, NS::StringEncoding::UTF8StringEncoding );

    if (defaultLibrary != nullptr)
    {
        shaders[ outShader.index ].computeProgram = defaultLibrary->newFunction( computeShaderName );
    }
    
    shaders[ outShader.index ].threadgroupCounts = MTL::Size::Make( threadsPerThreadgroupX, threadsPerThreadgroupY, 1 );
    
    if (shaders[ outShader.index ].computeProgram == nullptr)
    {
        shaders[ outShader.index ].computeProgram = shaderLibrary->newFunction( computeShaderName );

        if (shaders[ outShader.index ].computeProgram == nullptr)
        {
            //NSLog( @"Shader: Could not load %s!\n", name );
            printf( "Could not load compute shader!\n" );
            outShader.index = 0;
        }
    }

    NS::Error* error = nullptr;
    shaders[ outShader.index ].computePipeline = gDevice->newComputePipelineState( shaders[ outShader.index ].computeProgram, &error );
    
    if (!shaders[ outShader.index ].computePipeline)
    {
        //printf( "Error occurred when building compute pipeline for function %s: %s", name, [error localizedDescription] );
        printf( "error building compute pipeline\n" );
    }
    
    return outShader;
}

void teShaderDispatch( const teShader& shader, unsigned groupsX, unsigned groupsY, unsigned groupsZ, const ShaderParams& params, const char* debugName )
{
    teAssert( shader.index != 0 );
    teAssert( shaders[ shader.index ].computeProgram != nil );
    teAssert( !params.writeTexture || (TextureGetFlags( params.writeTexture ) & teTextureFlags::UAV ) );
    
    float m[ 16 ];
    UpdateUBO( m, m, m, m, params, Vec4( 0, 0, 0, 1 ), Vec4( 1, 1, 1, 1 ), Vec4( 1, 1, 1, 1 ) );
    
    MTL::Size threadgroups = MTL::Size::Make( groupsX, groupsY, groupsZ );

    MTL::CommandBuffer* commandBuffer = gCommandQueue->commandBuffer();
    commandBuffer->setLabel( NS::String::string( debugName, NS::UTF8StringEncoding ) );
    
    MTL::ComputeCommandEncoder* commandEncoder = commandBuffer->computeCommandEncoder();
    commandEncoder->setLabel( NS::String::string( debugName, NS::UTF8StringEncoding ) );
    
    commandEncoder->setTexture( TextureGetMetalTexture( params.readTexture ), 0 );
    commandEncoder->setTexture( TextureGetMetalTexture( params.writeTexture ), 1 );
    commandEncoder->setTexture( TextureGetMetalTexture( params.readTexture2 ), 2 );
    commandEncoder->setTexture( TextureGetMetalTexture( params.readTexture3 ), 3 );
    commandEncoder->setTexture( TextureGetMetalTexture( params.readTexture4 ), 4 );

    unsigned uboOffset = 0;
    MTL::Buffer* ubo = GetUniformBufferAndOffset( uboOffset );
    commandEncoder->setBuffer( ubo, uboOffset, 0 );
    commandEncoder->setComputePipelineState( shaders[ shader.index ].computePipeline );

    commandEncoder->dispatchThreadgroups( threadgroups, shaders[ shader.index ].threadgroupCounts );
    commandEncoder->endEncoding();
    
    commandBuffer->commit();
    
    MoveToNextUboOffset();
}
