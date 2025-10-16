#include "shader.h"
#include <vulkan/vulkan.h>
#include "file.h"
#include "te_stdlib.h"

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
void RegisterFileForModifications( const teFile& file, void(*updateFunc)(const char*) );
void ClearPSOCache();

struct teShaderImpl
{
    VkPipelineShaderStageCreateInfo vertexInfo = {};
    VkPipelineShaderStageCreateInfo meshInfo = {};
    VkPipelineShaderStageCreateInfo fragmentInfo = {};
    VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
    VkShaderModule meshShaderModule = VK_NULL_HANDLE;
    
    VkPipelineShaderStageCreateInfo computeInfo = {};
    VkShaderModule computeShaderModule = VK_NULL_HANDLE;
    VkPipeline computePso = {};
};

constexpr unsigned MaxShaders = 40;
teShaderImpl shaders[ MaxShaders ];
unsigned nextShaderIndex = 1;

struct ShaderCacheEntry
{
    char vertexPath[ 260 ] = {};
    char fragmentPath[ 260 ] = {};
    char vertexName[ 260 ] = {};
    char fragmentName[ 260 ] = {};
    char computePath[ 260 ] = {};
    char computeName[ 260 ] = {};
    unsigned shaderIndex = 0;
    inline static VkDevice device;
};

ShaderCacheEntry shaderCacheEntries[ MaxShaders ];

void teShaderGetInfo( const teShader& shader, VkPipelineShaderStageCreateInfo& outVertexInfo, VkPipelineShaderStageCreateInfo& outFragmentInfo, VkPipelineShaderStageCreateInfo& outMeshInfo )
{
    outVertexInfo = shaders[ shader.index ].vertexInfo;
    outFragmentInfo = shaders[ shader.index ].fragmentInfo;
    outMeshInfo = shaders[ shader.index ].meshInfo;
}

VkPipeline ShaderGetComputePSO( const teShader& shader )
{
    teAssert( shader.index != 0 );

    return shaders[ shader.index ].computePso;
}

void ReloadShader( const char* path )
{
    for (unsigned i = 0; i < MaxShaders; ++i)
    {
        if (!teStrcmp( shaderCacheEntries[ i ].vertexPath, path ) || !teStrcmp( shaderCacheEntries[ i ].fragmentPath, path ))
        {
            teFile vertexFile = teLoadFile( shaderCacheEntries[ i ].vertexPath );
            teFile fragmentFile = teLoadFile( shaderCacheEntries[ i ].fragmentPath );

            {
                VkShaderModuleCreateInfo moduleCreateInfo = {};
                moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                moduleCreateInfo.codeSize = vertexFile.size;
                moduleCreateInfo.pCode = (const uint32_t*)vertexFile.data;

                VK_CHECK( vkCreateShaderModule( ShaderCacheEntry::device, &moduleCreateInfo, nullptr, &shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexShaderModule ) );
                SetObjectName( ShaderCacheEntry::device, (uint64_t)shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, vertexFile.path );

                shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexInfo.module = shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexShaderModule;
                shaders[ shaderCacheEntries[ i ].shaderIndex ].vertexInfo.pName = shaderCacheEntries[ i ].vertexName;
            }

            {
                VkShaderModuleCreateInfo moduleCreateInfo = {};
                moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                moduleCreateInfo.codeSize = fragmentFile.size;
                moduleCreateInfo.pCode = (const uint32_t*)fragmentFile.data;

                VK_CHECK( vkCreateShaderModule( ShaderCacheEntry::device, &moduleCreateInfo, nullptr, &shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentShaderModule ) );
                SetObjectName( ShaderCacheEntry::device, (uint64_t)shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, fragmentFile.path );

                shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentInfo.module = shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentShaderModule;
                shaders[ shaderCacheEntries[ i ].shaderIndex ].fragmentInfo.pName = shaderCacheEntries[ i ].fragmentName;
            }
        }

        if (!teStrcmp( shaderCacheEntries[ i ].computePath, path ))
        {
            teFile file = teLoadFile( shaderCacheEntries[ i ].computePath );

            VkShaderModuleCreateInfo moduleCreateInfo = {};
            moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            moduleCreateInfo.codeSize = file.size;
            moduleCreateInfo.pCode = (const uint32_t*)file.data;

            VK_CHECK( vkCreateShaderModule( ShaderCacheEntry::device, &moduleCreateInfo, nullptr, &shaders[ shaderCacheEntries[ i ].shaderIndex ].computeShaderModule ) );
            SetObjectName( ShaderCacheEntry::device, (uint64_t)shaders[ shaderCacheEntries[ i ].shaderIndex ].computeShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, file.path );

            shaders[ shaderCacheEntries[ i ].shaderIndex ].computeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaders[ shaderCacheEntries[ i ].shaderIndex ].computeInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shaders[ shaderCacheEntries[ i ].shaderIndex ].computeInfo.module = shaders[ shaderCacheEntries[ i ].shaderIndex ].computeShaderModule;
            shaders[ shaderCacheEntries[ i ].shaderIndex ].computeInfo.pName = shaderCacheEntries[ i ].computeName;
        }
    }

    ClearPSOCache();
}

teShader teCreateShader( VkDevice device, const struct teFile& vertexFile, const struct teFile& fragmentFile, const char* vertexName, const char* fragmentName )
{
    teAssert( nextShaderIndex < MaxShaders );

    ShaderCacheEntry::device = device;

    teShader outShader;
    outShader.index = nextShaderIndex++;

    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = vertexFile.size;
        moduleCreateInfo.pCode = (const uint32_t*)vertexFile.data;

        VK_CHECK( vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].vertexShaderModule ) );
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

        VK_CHECK( vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].fragmentShaderModule ) );
        SetObjectName( device, (uint64_t)shaders[ outShader.index ].fragmentShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, fragmentFile.path );

        shaders[ outShader.index ].fragmentInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaders[ outShader.index ].fragmentInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaders[ outShader.index ].fragmentInfo.module = shaders[ outShader.index ].fragmentShaderModule;
        shaders[ outShader.index ].fragmentInfo.pName = fragmentName;
    }

    RegisterFileForModifications( vertexFile, ReloadShader );
    RegisterFileForModifications( fragmentFile, ReloadShader );

    for (unsigned i = 0; i < MaxShaders; ++i)
    {
        if (shaderCacheEntries[ i ].shaderIndex == 0)
        {
            shaderCacheEntries[ i ].shaderIndex = outShader.index;
            teMemcpy( shaderCacheEntries[ i ].vertexName, vertexName, teStrlen( vertexName ) + 1 );
            teMemcpy( shaderCacheEntries[ i ].fragmentName, fragmentName, teStrlen( fragmentName ) + 1 );
            teMemcpy( shaderCacheEntries[ i ].vertexPath, vertexFile.path, teStrlen( vertexFile.path ) + 1 );
            teMemcpy( shaderCacheEntries[ i ].fragmentPath, fragmentFile.path, teStrlen( fragmentFile.path ) + 1 );
            break;
        }
    }

    return outShader;
}

teShader teCreateMeshShader( VkDevice device, const teFile& meshShaderFile, const teFile& fragmentShaderFile, const char* meshShaderName, const char* fragmentShaderName )
{
    teAssert( nextShaderIndex < MaxShaders );

    ShaderCacheEntry::device = device;

    teShader outShader;
    outShader.index = nextShaderIndex++;

    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = meshShaderFile.size;
        moduleCreateInfo.pCode = (const uint32_t*)meshShaderFile.data;

        VK_CHECK( vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].meshShaderModule ) );
        SetObjectName( device, (uint64_t)shaders[ outShader.index ].meshShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, meshShaderFile.path );

        shaders[ outShader.index ].meshInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaders[ outShader.index ].meshInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
        shaders[ outShader.index ].meshInfo.module = shaders[ outShader.index ].meshShaderModule;
        shaders[ outShader.index ].meshInfo.pName = meshShaderName;
    }

    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = fragmentShaderFile.size;
        moduleCreateInfo.pCode = (const uint32_t*)fragmentShaderFile.data;

        VK_CHECK( vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].fragmentShaderModule ) );
        SetObjectName( device, (uint64_t)shaders[ outShader.index ].fragmentShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, fragmentShaderFile.path );

        shaders[ outShader.index ].fragmentInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaders[ outShader.index ].fragmentInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaders[ outShader.index ].fragmentInfo.module = shaders[ outShader.index ].fragmentShaderModule;
        shaders[ outShader.index ].fragmentInfo.pName = fragmentShaderName;
    }

    return outShader;
}

teShader teCreateComputeShader( VkDevice device, VkPipelineLayout pipelineLayout, const teFile& file, const char* name, unsigned /*threadsPerThreadgroupX*/, unsigned /*threadsPerThreadgroupY*/ )
{
    teAssert( nextShaderIndex < MaxShaders );

    ShaderCacheEntry::device = device;

    teShader outShader;
    outShader.index = nextShaderIndex++;

    if (file.size == 0)
    {
        tePrint( "teCreateComputeShader failed because file '%s' is empty!\n", file.path );
        teAssert( false );
        return outShader;
    }

    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = file.size;
    moduleCreateInfo.pCode = (const uint32_t*)file.data;

    VK_CHECK( vkCreateShaderModule( device, &moduleCreateInfo, nullptr, &shaders[ outShader.index ].vertexShaderModule ) );
    SetObjectName( device, (uint64_t)shaders[ outShader.index ].vertexShaderModule, VK_OBJECT_TYPE_SHADER_MODULE, file.path );

    shaders[ outShader.index ].computeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[ outShader.index ].computeInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaders[ outShader.index ].computeInfo.module = shaders[ outShader.index ].vertexShaderModule;
    shaders[ outShader.index ].computeInfo.pName = name;

    VkComputePipelineCreateInfo psoInfo = {};
    psoInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    psoInfo.layout = pipelineLayout;
    psoInfo.stage = shaders[ outShader.index ].computeInfo;

    VK_CHECK( vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &psoInfo, nullptr, &shaders[ outShader.index ].computePso ) );
    SetObjectName( device, (uint64_t)shaders[ outShader.index ].computePso, VK_OBJECT_TYPE_PIPELINE, file.path );

    RegisterFileForModifications( file, ReloadShader );

    for (unsigned i = 0; i < MaxShaders; ++i)
    {
        if (shaderCacheEntries[ i ].shaderIndex == 0)
        {
            shaderCacheEntries[ i ].shaderIndex = outShader.index;
            teMemcpy( shaderCacheEntries[ i ].computeName, name, teStrlen( name ) + 1 );
            teMemcpy( shaderCacheEntries[ i ].computePath, file.path, teStrlen( file.path ) + 1 );
            break;
        }
    }

    return outShader;
}
