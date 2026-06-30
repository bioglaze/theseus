#pragma once

struct teShader
{
    unsigned index = 0;
};

struct ShaderParams
{
    alignas(16) float clipToView[ 16  ];
    alignas(16) float localToView[ 16 ];
    unsigned readTexture;
    unsigned readTexture2;
    unsigned readTexture3;
    unsigned readTexture4;
    unsigned writeTexture;
    float bloomThreshold;
    float tilesXY[ 4 ];
    float tint[ 4 ];
    unsigned readBuffer;
    unsigned readBuffer3;
    unsigned writeBuffer;
};

/*
    When using Vulkan, vertexFile and FragmentFile must point to a SPIR-V shader. vertexName is the name of the vertex shader main function.
*/
teShader teCreateShader( const struct teFile& vertexFile, const teFile& fragmentFile, const char* vertexName, const char* fragmentName );
teShader teCreateMeshShader( const teFile& meshShaderFile, const teFile& fragmentShaderFile, const char* meshShaderName, const char* fragmentShaderName );
teShader teCreateComputeShader( const teFile& file, const char* name, unsigned threadsPerThreadgroupX, unsigned threadsPerThreadgroupY );
void teShaderDispatch( const teShader& shader, unsigned groupsX, unsigned groupsY, unsigned groupsZ, const ShaderParams& params, const char* debugName );
