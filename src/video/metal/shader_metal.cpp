#include <Metal/Metal.hpp>
#include "shader.h"
#include "texture.h"
#include "te_stdlib.h"
#include "vec3.h"

void MoveToNextUboOffset();
unsigned TextureGetFlags( unsigned index );

extern MTL::Library* shaderLibrary;
extern MTL::CommandQueue* gCommandQueue;
extern MTL::Device* gDevice;

MTL::Texture* TextureGetMetalTexture( unsigned index );
MTL::Buffer* GetUniformBufferAndOffset( unsigned& outOffset );
teBuffer GetPointLightCenterAndRadiusBuffer();
teBuffer GetLightIndexBuffer();

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

    if (shaders[ outShader.index ].vertexProgram == nullptr)
    {
        shaders[ outShader.index ].vertexProgram = shaderLibrary->newFunction( vertexShaderName );

        if (shaders[ outShader.index ].vertexProgram == nullptr)
        {
            printf( "Could not load vertex shader '%s'\n", vertexName );
            outShader.index = 0;
            return outShader;
        }
    }
    
    NS::String* pixelShaderName = NS::String::string( pixelName, NS::StringEncoding::UTF8StringEncoding );

    if (shaders[ outShader.index ].pixelProgram == nullptr)
    {
        shaders[ outShader.index ].pixelProgram = shaderLibrary->newFunction( pixelShaderName );

        if (shaders[ outShader.index ].pixelProgram == nullptr)
        {
            printf( "could not load pixel shader '%s'\n", pixelName );
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

    shaders[ outShader.index ].threadgroupCounts = MTL::Size::Make( threadsPerThreadgroupX, threadsPerThreadgroupY, 1 );
    
    if (shaders[ outShader.index ].computeProgram == nullptr)
    {
        shaders[ outShader.index ].computeProgram = shaderLibrary->newFunction( computeShaderName );

        if (shaders[ outShader.index ].computeProgram == nullptr)
        {
            printf( "Could not load compute shader '%s'!\n", name );
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
    UpdateUBO( m, m, m, params, Vec4( 0, 0, 0, 1 ), Vec4( 1, 1, 1, 1 ), Vec4( 1, 1, 1, 1 ) );
    
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

    // FIXME: implement
    /*teBuffer readBuf;
    readBuf.index = params.readBuffer;
    MTL::Buffer* readBuffer = BufferGetBuffer( readBuf );
    
    if (readBuffer)
    {
        commandEncoder->setBuffer( readBuffer, 0, 1 );
    }
    
    teBuffer writeBuf;
    writeBuf.index = params.writeBuffer;
    MTL::Buffer* writeBuffer = BufferGetBuffer( writeBuf );
    
    if(writeBuffer)
    {
        commandEncoder->setBuffer( writeBuffer, 0, 2 );        
    }*/

    // FIXME: bind point light color buffer and light index buffer for standard shader.
    
    commandEncoder->setComputePipelineState( shaders[ shader.index ].computePipeline );

    commandEncoder->dispatchThreadgroups( threadgroups, shaders[ shader.index ].threadgroupCounts );
    commandEncoder->endEncoding();
    
    commandBuffer->commit();
    
    MoveToNextUboOffset();
}
