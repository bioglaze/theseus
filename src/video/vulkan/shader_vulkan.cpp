#include "shader.h"
#include <vulkan/vulkan.h>
#include "file.h"

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );

struct teShaderImpl
{
    VkPipelineShaderStageCreateInfo vertexInfo = {};
    VkPipelineShaderStageCreateInfo fragmentInfo = {};
    VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
    VkPipeline pso = {};
};

teShaderImpl shaders[ 40 ];
unsigned shaderCount = 0;

void teShaderGetInfo( const teShader& shader, VkPipelineShaderStageCreateInfo& outVertexInfo, VkPipelineShaderStageCreateInfo& outFragmentInfo )
{
    outVertexInfo = shaders[ shader.index ].vertexInfo;
    outFragmentInfo = shaders[ shader.index ].fragmentInfo;
}

teShader teCreateShader( VkDevice device, const struct teFile& vertexFile, const struct teFile& fragmentFile, const char* vertexName, const char* fragmentName )
{
    teAssert( shaderCount < 40 );

    teShader outShader;
    outShader.index = shaderCount++;

    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = vertexFile.size;
        moduleCreateInfo.pCode = (const uint32_t*)vertexFile.data;

        VkResult err = vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].vertexShaderModule );
        teAssert( err == VK_SUCCESS );
        SetObjectName( device, (uint64_t)shaders[ outShader.index ].vertexShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, vertexFile.path );

        shaders[ outShader.index ].vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaders[ outShader.index ].vertexInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaders[ outShader.index ].vertexInfo.module = shaders[ outShader.index ].vertexShaderModule;
        shaders[ outShader.index ].vertexInfo.pName = vertexName;
    }

    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = fragmentFile.size;
        moduleCreateInfo.pCode = (const uint32_t*)fragmentFile.data;

        VkResult err = vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].fragmentShaderModule );
        teAssert( err == VK_SUCCESS );
        SetObjectName( device, (uint64_t)shaders[ outShader.index ].fragmentShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, fragmentFile.path );

        shaders[ outShader.index ].fragmentInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaders[ outShader.index ].fragmentInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaders[ outShader.index ].fragmentInfo.module = shaders[ outShader.index ].fragmentShaderModule;
        shaders[ outShader.index ].fragmentInfo.pName = fragmentName;
    }

    return outShader;
}
