#import <Metal/Metal.h>
#include "shader.h"
#include "te_stdlib.h"

extern id <MTLLibrary> defaultLibrary;

struct ShaderImpl
{
    id <MTLFunction> vertexProgram;
    id <MTLFunction> pixelProgram;
};

static constexpr unsigned MaxShaders = 40;
static ShaderImpl shaders[ MaxShaders ];
static unsigned shaderCount = 0;

teShader teCreateShader( const struct teFile& vertexFile, const teFile& pixelFile, const char* vertexName, const char* pixelName )
{
    teAssert( shaderCount < MaxShaders );

    teShader outShader;
    outShader.index = ++shaderCount;
    
    NSString* vertexShaderName = [NSString stringWithUTF8String:vertexName ];
    shaders[ outShader.index ].vertexProgram = [defaultLibrary newFunctionWithName:vertexShaderName];
    
    if (shaders[ outShader.index ].vertexProgram == nullptr)
    {
        NSLog( @"Shader: Could not load %s!\n", vertexName );
        outShader.index = 0;
        return outShader;
    }
    
    NSString* pixelShaderName = [NSString stringWithUTF8String:pixelName ];
    shaders[ outShader.index ].pixelProgram = [defaultLibrary newFunctionWithName:pixelShaderName];
    
    if (shaders[ outShader.index ].pixelProgram == nullptr)
    {
        NSLog( @"Shader: Could not load %s!\n", pixelName );
        outShader.index = -1;
    }

    return outShader;
}

