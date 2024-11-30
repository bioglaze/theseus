#import <Metal/Metal.h>
#include "shader.h"
#include "texture.h"
#include "te_stdlib.h"
#include "vec3.h"

void UpdateUBO( const float localToClip[ 16 ], const float localToView[ 16 ], const float localToShadowClip[ 16 ], const ShaderParams& shaderParams, const Vec4& lightDir );
void MoveToNextUboOffset();
unsigned TextureGetFlags( unsigned index );

extern id <MTLLibrary> defaultLibrary;
extern id <MTLLibrary> shaderLibrary;
extern id <MTLCommandQueue> gCommandQueue;
extern id<MTLDevice> gDevice;

id<MTLTexture> TextureGetMetalTexture( unsigned index );
id<MTLBuffer> GetUniformBufferAndOffset( unsigned& outOffset );

struct ShaderImpl
{
    id <MTLFunction> vertexProgram;
    id <MTLFunction> pixelProgram;
    id <MTLFunction> computeProgram;
    id <MTLComputePipelineState> computePipeline;
    unsigned threadsPerThreadgroupX;
    unsigned threadsPerThreadgroupY;
    MTLSize threadgroupCounts;
};

static constexpr unsigned MaxShaders = 40;
static ShaderImpl shaders[ MaxShaders ];
static unsigned shaderCount = 0;

id< MTLFunction > teShaderGetVertexProgram( const teShader& shader )
{
    return shaders[ shader.index ].vertexProgram;
}

id< MTLFunction > teShaderGetPixelProgram( const teShader& shader )
{
    return shaders[ shader.index ].pixelProgram;
}

teShader teCreateShader( const struct teFile& vertexFile, const teFile& pixelFile, const char* vertexName, const char* pixelName )
{
    teAssert( shaderCount < MaxShaders );

    teShader outShader;
    outShader.index = ++shaderCount;
    
    NSString* vertexShaderName = [NSString stringWithUTF8String:vertexName ];
    shaders[ outShader.index ].vertexProgram = [defaultLibrary newFunctionWithName:vertexShaderName];
    
    if (shaders[ outShader.index ].vertexProgram == nullptr)
    {
        shaders[ outShader.index ].vertexProgram = [shaderLibrary newFunctionWithName:vertexShaderName];

        if (shaders[ outShader.index ].vertexProgram == nullptr)
        {
            NSLog( @"Shader: Could not load %s!\n", vertexName );
            outShader.index = 0;
            return outShader;
        }
    }
    
    NSString* pixelShaderName = [NSString stringWithUTF8String:pixelName ];
    shaders[ outShader.index ].pixelProgram = [defaultLibrary newFunctionWithName:pixelShaderName];
    
    if (shaders[ outShader.index ].pixelProgram == nullptr)
    {
        shaders[ outShader.index ].pixelProgram = [shaderLibrary newFunctionWithName:pixelShaderName];

        if (shaders[ outShader.index ].pixelProgram == nullptr)
        {
            NSLog( @"Shader: Could not load %s!\n", pixelName );
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
    
    NSString* computeShaderName = [NSString stringWithUTF8String:name ];
    shaders[ outShader.index ].computeProgram = [defaultLibrary newFunctionWithName:computeShaderName];
    shaders[ outShader.index ].threadgroupCounts = MTLSizeMake( threadsPerThreadgroupX, threadsPerThreadgroupY, 1 );
    
    if (shaders[ outShader.index ].computeProgram == nullptr)
    {
        shaders[ outShader.index ].computeProgram = [shaderLibrary newFunctionWithName:computeShaderName];

        if (shaders[ outShader.index ].computeProgram == nullptr)
        {
            NSLog( @"Shader: Could not load %s!\n", name );
            outShader.index = 0;
        }
    }

    NSError *error = nil;
    shaders[ outShader.index ].computePipeline = [gDevice newComputePipelineStateWithFunction:shaders[ outShader.index ].computeProgram error:&error];
    
    if (!shaders[ outShader.index ].computePipeline)
    {
        NSLog( @"Error occurred when building compute pipeline for function %s: %@", name, [error localizedDescription] );
    }
    
    return outShader;
}

void teShaderDispatch( const teShader& shader, unsigned groupsX, unsigned groupsY, unsigned groupsZ, const ShaderParams& params, const char* debugName )
{
    teAssert( shader.index != 0 );
    teAssert( shaders[ shader.index ].computeProgram != nil );
    teAssert( !params.writeTexture || (TextureGetFlags( params.writeTexture ) & teTextureFlags::UAV ) );
    
    float m[ 16 ];
    UpdateUBO( m, m, m, params, Vec4( 0, 0, 0, 1 ) );
    
    MTLSize threadgroups = MTLSizeMake( groupsX, groupsY, groupsZ );

    id<MTLCommandBuffer> commandBuffer = [gCommandQueue commandBuffer];
    commandBuffer.label = [NSString stringWithUTF8String:debugName ];
    
    id<MTLComputeCommandEncoder> commandEncoder = [commandBuffer computeCommandEncoder];
    commandEncoder.label = commandBuffer.label;
    
    [commandEncoder setTexture:TextureGetMetalTexture( params.readTexture ) atIndex:0];
    [commandEncoder setTexture:TextureGetMetalTexture( params.writeTexture ) atIndex:1];

    unsigned uboOffset = 0;
    id<MTLBuffer> ubo = GetUniformBufferAndOffset( uboOffset );
    [commandEncoder setBuffer:ubo offset:uboOffset atIndex:0];
    [commandEncoder setComputePipelineState:shaders[ shader.index ].computePipeline];

    [commandEncoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:shaders[ shader.index ].threadgroupCounts];
    [commandEncoder endEncoding];
    
    [commandBuffer commit];
    
    MoveToNextUboOffset();
}
