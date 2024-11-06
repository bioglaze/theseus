#pragma once

struct teShader
{
    unsigned index = 0;
};

struct ShaderParams
{
    unsigned readTexture;
    unsigned writeTexture;
    float bloomThreshold;
    float tilesXY[ 4 ];
    float tint[ 4 ];
};

/*
    When using Vulkan, vertexFile and FragmentFile must point to a SPIR-V shader. vertexName is the name of the vertex shader main function.
*/
teShader teCreateShader( const struct teFile& vertexFile, const teFile& fragmentFile, const char* vertexName, const char* fragmentName );
teShader teCreateComputeShader( const teFile& file, const char* name, unsigned threadsPerThreadgroupX, unsigned threadsPerThreadgroupY );
void teShaderDispatch( const teShader& shader, unsigned groupsX, unsigned groupsY, unsigned groupsZ, const ShaderParams& params, const char* debugName );
