#pragma once

struct teShader
{
    unsigned index = 0;
};

/*
    When using Vulkan, vertexFile and FragmentFile must point to a SPIR-V shader. vertexName is the name of the vertex shader main function.
*/
teShader teCreateShader( const struct teFile& vertexFile, const teFile& fragmentFile, const char* vertexName, const char* fragmentName );
