#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include "buffer.h"
#include "camera.h"
#include "file.h"
#include "material.h"
#include "matrix.h"
#include "texture.h"
#include "te_stdlib.h"
#include "shader.h"
#include "vec3.h"

teShader teCreateShader( VkDevice device, const struct teFile& vertexFile, const struct teFile& fragmentFile, const char* vertexName, const char* fragmentName );
teShader teCreateComputeShader( VkDevice device, VkPipelineLayout pipelineLayout, const teFile& file, const char* name, unsigned /*threadsPerThreadgroupX*/, unsigned /*threadsPerThreadgroupY*/ );
void teShaderGetInfo( const teShader& shader, VkPipelineShaderStageCreateInfo& outVertexInfo, VkPipelineShaderStageCreateInfo& outFragmentInfo );
VkPipeline ShaderGetComputePSO( const teShader& shader );
unsigned GetMemoryUsage( unsigned width, unsigned height, VkFormat format );
teTexture2D teCreateTexture2D( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName );
teTextureCube teCreateTextureCube( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned dimension, unsigned flags, teTextureFormat format, const char* debugName );
teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags,
    VkDevice device, VkBuffer* stagingBuffers, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkQueue graphicsQueue, VkCommandBuffer cmdBuffer );
teTexture2D teLoadTexture( const struct teFile& file, unsigned flags, VkDevice device, VkBuffer stagingBuffer, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkQueue graphicsQueue, VkCommandBuffer cmdBuffer, const VkPhysicalDeviceProperties& properties,
                           void* pixels, int pixelsWidth, int pixelsHeight, teTextureFormat pixelsFormat );
VkImageView TextureGetView( teTexture2D texture );
VkImage TextureGetImage( teTexture2D texture );
void GetFormatAndBPP( teTextureFormat bcFormat, VkFormat& outFormat, unsigned& outBytesPerPixel );
Buffer CreateBuffer( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned sizeBytes, VkMemoryPropertyFlags memoryFlags, VkBufferUsageFlags usageFlags, BufferViewType viewType, const char* debugName );
VkBufferView BufferGetView( const Buffer& buffer );
VkDeviceMemory BufferGetMemory( const Buffer& buffer );
VkBuffer BufferGetBuffer( const Buffer& buffer );
void WaylandDispatch();

extern struct wl_display* gwlDisplay;
extern struct wl_surface* gwlSurface;

constexpr unsigned DescriptorEntryCount = 7;
constexpr unsigned SamplerCount = 6;

// Must match ubo.h shader header!
struct PushConstants
{
    int textureIndex;
    int shadowTextureIndex;
    int normalMapIndex;
    int unused;
    float scale[ 2 ];
    float translate[ 2 ];
};

uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties )
{
    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((typeBits & (1 << i)) != 0 && (deviceMemoryProperties.memoryTypes[ i ].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    teAssert( !"could not get memory type" );
    return 0;
}

struct PSO
{
    VkPipeline pso = VK_NULL_HANDLE;
    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    teBlendMode blendMode = teBlendMode::Off;
    teCullMode cullMode = teCullMode::Off;
    teDepthMode depthMode = teDepthMode::NoneWriteOff;
    teFillMode fillMode = teFillMode::Solid;
    teTopology topology = teTopology::Triangles;
    teTextureFormat colorFormat = teTextureFormat::Invalid;
    teTextureFormat depthFormat = teTextureFormat::Invalid;
    bool isUI = false;
};

struct PerObjectUboStruct
{
    Matrix localToClip;
    Matrix localToView;
    Matrix localToShadowClip;
    Vec4 bloomParams;
    Vec4 tilesXY;
};

struct Ubo
{
    uint8_t* uboData = nullptr;
    Buffer buffer;
    size_t offset = 0;
};

struct SwapchainResource
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkCommandBuffer drawCommandBuffer = VK_NULL_HANDLE;
    VkSemaphore renderCompleteSemaphore = VK_NULL_HANDLE;
    VkSemaphore imageAcquiredSemaphore = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkImage depthStencilImage = VK_NULL_HANDLE;
    VkDeviceMemory depthStencilMem = VK_NULL_HANDLE;
    VkImageView depthStencilView = VK_NULL_HANDLE;
    Ubo ubo;
    teTextureFormat colorFormat = teTextureFormat::Invalid;
    teTextureFormat depthFormat = teTextureFormat::Invalid;
    static constexpr unsigned SetCount = 1000;
    unsigned setIndex = 0;
    VkDescriptorSet descriptorSets[ SetCount ] = {};
};

struct Renderer
{
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties = {};
    VkQueue graphicsQueue;
    VkCommandPool cmdPool;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorImageInfo samplerInfos[ TextureCount ];
    VkPipelineLayout pipelineLayout;

    teBuffer staticMeshPositionBuffer;
    teBuffer staticMeshPositionStagingBuffer;
    teBuffer staticMeshUVBuffer;
    teBuffer staticMeshUVStagingBuffer;
    teBuffer staticMeshNormalBuffer;
    teBuffer staticMeshNormalStagingBuffer;
    teBuffer staticMeshTangentBuffer;
    teBuffer staticMeshTangentStagingBuffer;
    teBuffer staticMeshIndexBuffer;
    teBuffer staticMeshIndexStagingBuffer;
    teBuffer uiVertexBuffer;
    teBuffer uiIndexBuffer;
    float* uiVertices = nullptr;
    uint16_t* uiIndices = nullptr;
    unsigned indexCounter = 0;
    unsigned uvCounter = 0;
    unsigned positionCounter = 0;
    unsigned normalCounter = 0;
    unsigned tangentCounter = 0;
    teTextureFormat currentColorFormat = teTextureFormat::Invalid;
    teTextureFormat currentDepthFormat = teTextureFormat::Invalid;

    teTexture2D defaultTexture2D;
    teTextureCube defaultTextureCube;
    teTexture2D nullUAV;
    teBuffer nullBuffer;
    VkSampler samplerLinearRepeat;
    VkSampler samplerLinearClamp;
    VkSampler samplerAnisotropic8Repeat;
    VkSampler samplerAnisotropic8Clamp;
    VkSampler samplerNearestRepeat;
    VkSampler samplerNearestClamp;

    SwapchainResource swapchainResources[ 4 ] = {};
    uint32_t swapchainImageCount = 0;
    uint32_t graphicsQueueIndex = 0;
    unsigned currentBuffer = 0;
    unsigned frameIndex = 0;
    unsigned swapchainWidth = 0;
    unsigned swapchainHeight = 0;

    VkBuffer textureStagingBuffers[ 6 ];
    VkDeviceMemory textureStagingMemories[ 6 ];
    VkMemoryAllocateInfo textureStagingMemAllocInfos[ 6 ];

    static constexpr unsigned MaxPSOs = 250;
    PSO psos[ MaxPSOs ];
    VkPipeline boundPSO = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT                      dbgMessenger = VK_NULL_HANDLE;
    PFN_vkSetDebugUtilsObjectNameEXT              SetDebugUtilsObjectNameEXT = VK_NULL_HANDLE;
    PFN_vkCmdBeginDebugUtilsLabelEXT              CmdBeginDebugUtilsLabelEXT = VK_NULL_HANDLE;
    PFN_vkCmdEndDebugUtilsLabelEXT                CmdEndDebugUtilsLabelEXT = VK_NULL_HANDLE;
    PFN_vkCreateSwapchainKHR                      createSwapchainKHR = VK_NULL_HANDLE;
    PFN_vkGetSwapchainImagesKHR                   getSwapchainImagesKHR = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilitiesKHR = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPhysicalDeviceSurfacePresentModesKHR = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR      getPhysicalDeviceSurfaceSupportKHR = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      getPhysicalDeviceSurfaceFormatsKHR = VK_NULL_HANDLE;
    PFN_vkAcquireNextImageKHR                     acquireNextImageKHR = VK_NULL_HANDLE;
    PFN_vkQueuePresentKHR                         queuePresentKHR = VK_NULL_HANDLE;

    ShaderParams shaderParams;

    unsigned statDrawCalls = 0;
    unsigned statPSOBinds = 0;

    bool meshShaderSupported = false;
    
    static constexpr unsigned uboSizeBytes = sizeof( PerObjectUboStruct ) * 10000;
};

Renderer renderer;

const char* getObjectType( VkObjectType type )
{
    switch (type)
    {
    case VK_OBJECT_TYPE_QUERY_POOL: return "VK_OBJECT_TYPE_QUERY_POOL";
    case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION: return "VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION";
    case VK_OBJECT_TYPE_SEMAPHORE: return "VK_OBJECT_TYPE_SEMAPHORE";
    case VK_OBJECT_TYPE_SHADER_MODULE: return "VK_OBJECT_TYPE_SHADER_MODULE";
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "VK_OBJECT_TYPE_SWAPCHAIN_KHR";
    case VK_OBJECT_TYPE_SAMPLER: return "VK_OBJECT_TYPE_SAMPLER";
    case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT: return "VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT";
    case VK_OBJECT_TYPE_IMAGE: return "VK_OBJECT_TYPE_IMAGE";
    case VK_OBJECT_TYPE_UNKNOWN: return "VK_OBJECT_TYPE_UNKNOWN";
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "VK_OBJECT_TYPE_DESCRIPTOR_POOL";
    case VK_OBJECT_TYPE_COMMAND_BUFFER: return "VK_OBJECT_TYPE_COMMAND_BUFFER";
    case VK_OBJECT_TYPE_BUFFER: return "VK_OBJECT_TYPE_BUFFER";
    case VK_OBJECT_TYPE_SURFACE_KHR: return "VK_OBJECT_TYPE_SURFACE_KHR";
    case VK_OBJECT_TYPE_INSTANCE: return "VK_OBJECT_TYPE_INSTANCE";
    case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT: return "VK_OBJECT_TYPE_VALIDATION_CACHE_EXT";
    case VK_OBJECT_TYPE_IMAGE_VIEW: return "VK_OBJECT_TYPE_IMAGE_VIEW";
    case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "VK_OBJECT_TYPE_DESCRIPTOR_SET";
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: return "VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT";
    case VK_OBJECT_TYPE_COMMAND_POOL: return "VK_OBJECT_TYPE_COMMAND_POOL";
    case VK_OBJECT_TYPE_PHYSICAL_DEVICE: return "VK_OBJECT_TYPE_PHYSICAL_DEVICE";
    case VK_OBJECT_TYPE_DISPLAY_KHR: return "VK_OBJECT_TYPE_DISPLAY_KHR";
    case VK_OBJECT_TYPE_BUFFER_VIEW: return "VK_OBJECT_TYPE_BUFFER_VIEW";
    case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT: return "VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT";
    case VK_OBJECT_TYPE_FRAMEBUFFER: return "VK_OBJECT_TYPE_FRAMEBUFFER";
    case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE: return "VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE";
    case VK_OBJECT_TYPE_PIPELINE_CACHE: return "VK_OBJECT_TYPE_PIPELINE_CACHE";
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "VK_OBJECT_TYPE_PIPELINE_LAYOUT";
    case VK_OBJECT_TYPE_DEVICE_MEMORY: return "VK_OBJECT_TYPE_DEVICE_MEMORY";
    case VK_OBJECT_TYPE_FENCE: return "VK_OBJECT_TYPE_FENCE";
    case VK_OBJECT_TYPE_QUEUE: return "VK_OBJECT_TYPE_QUEUE";
    case VK_OBJECT_TYPE_DEVICE: return "VK_OBJECT_TYPE_DEVICE";
    case VK_OBJECT_TYPE_RENDER_PASS: return "VK_OBJECT_TYPE_RENDER_PASS";
    case VK_OBJECT_TYPE_DISPLAY_MODE_KHR: return "VK_OBJECT_TYPE_DISPLAY_MODE_KHR";
    case VK_OBJECT_TYPE_EVENT: return "VK_OBJECT_TYPE_EVENT";
    case VK_OBJECT_TYPE_PIPELINE: return "VK_OBJECT_TYPE_PIPELINE";
    default: return "unhandled type";
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL dbgFunc( VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity, VkDebugUtilsMessageTypeFlagsEXT msgType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* /*userData*/ )
{
    if (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf( "ERROR: %s\n", callbackData->pMessage );
    }
    else if (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf( "WARNING: %s\n", callbackData->pMessage );
    }
    else if (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        printf( "INFO: %s\n", callbackData->pMessage );
    }
    else if (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        printf( "VERBOSE: %s\n", callbackData->pMessage );
    }

    if (msgType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
    {
        printf( "GENERAL: %s\n", callbackData->pMessage );
    }
    else if (msgType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        //printf( "PERF: %s\n", callbackData->pMessage );
    }

    if (callbackData->objectCount > 0)
    {
        for (uint32_t l = 0; l < callbackData->cmdBufLabelCount; ++l)
        {
            printf( "Command Buffer label: %s\n", callbackData->pCmdBufLabels[ l ].pLabelName );
        }

        for (uint32_t i = 0; i < callbackData->objectCount; ++i)
        {
            const char* name = callbackData->pObjects[ i ].pObjectName ? callbackData->pObjects[ i ].pObjectName : "unnamed";
            printf( "Object %u: name: %s, type: %s\n", i, name, getObjectType( callbackData->pObjects[ i ].objectType ) );
        }
    }
    
    if (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        teAssert( !"Vulkan debug message" );
    }

    return VK_FALSE;
}

void DebugBarrier( VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout )
{
    if (oldImageLayout == newImageLayout)
    {
        return;
    }

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    vkCmdPipelineBarrier( cmdbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, destStageFlags, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
}

void SetImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout, unsigned layerCount, unsigned mipLevel, unsigned mipLevelCount, VkPipelineStageFlags srcStageFlags )
{
    if (oldImageLayout == newImageLayout)
    {
        return;
    }

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
    imageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
    imageMemoryBarrier.subresourceRange.levelCount = mipLevelCount;
    imageMemoryBarrier.subresourceRange.layerCount = layerCount;

    switch (oldImageLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        imageMemoryBarrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        break;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        if (imageMemoryBarrier.srcAccessMask == 0 && (srcStageFlags & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT))
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        }
        else if (imageMemoryBarrier.srcAccessMask == 0)
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }

        if (srcStageFlags == VK_PIPELINE_STAGE_TRANSFER_BIT)
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }

        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    }

    VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (imageMemoryBarrier.dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT)
    {
        destStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (imageMemoryBarrier.dstAccessMask == VK_ACCESS_SHADER_READ_BIT)
    {
        destStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (imageMemoryBarrier.dstAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    {
        destStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    if (imageMemoryBarrier.dstAccessMask == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
    {
        destStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    if (imageMemoryBarrier.dstAccessMask == VK_ACCESS_TRANSFER_READ_BIT)
    {
        destStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (imageMemoryBarrier.dstAccessMask == VK_ACCESS_SHADER_WRITE_BIT)
    {
        destStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    vkCmdPipelineBarrier( cmdbuffer, srcStageFlags, destStageFlags, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
}

static void CreateStagingTexture( unsigned index )
{
    teAssert( index < 6 );

    const VkDeviceSize imageSize = 4096 * 4096 * 4;

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = imageSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK( vkCreateBuffer( renderer.device, &bufferCreateInfo, nullptr, &renderer.textureStagingBuffers[ index ] ) );
    SetObjectName( renderer.device, (uint64_t)renderer.textureStagingBuffers[ index ], VK_OBJECT_TYPE_BUFFER, "texture staging buffer" );

    VkMemoryRequirements memReqs = {};
    vkGetBufferMemoryRequirements( renderer.device, renderer.textureStagingBuffers[ index ], &memReqs );

    renderer.textureStagingMemAllocInfos[ index ].sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    renderer.textureStagingMemAllocInfos[ index ].allocationSize = (memReqs.size + renderer.properties.limits.nonCoherentAtomSize - 1) & ~(renderer.properties.limits.nonCoherentAtomSize - 1);;
    renderer.textureStagingMemAllocInfos[ index ].memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, renderer.deviceMemoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
    VK_CHECK( vkAllocateMemory( renderer.device, &renderer.textureStagingMemAllocInfos[ index ], nullptr, &renderer.textureStagingMemories[ index ] ) );
    SetObjectName( renderer.device, (uint64_t)renderer.textureStagingMemories[ index ], VK_OBJECT_TYPE_DEVICE_MEMORY, "texture staging memory" );

    VK_CHECK( vkBindBufferMemory( renderer.device, renderer.textureStagingBuffers[ index ], renderer.textureStagingMemories[ index ], 0 ) );
}

void UpdateStagingTexture( const uint8_t* src, unsigned width, unsigned height, VkFormat format, unsigned index )
{
    const VkDeviceSize imageSize = GetMemoryUsage( width, height, format );

    void* stagingData;
    VK_CHECK( vkMapMemory( renderer.device, renderer.textureStagingMemories[ index ], 0, renderer.textureStagingMemAllocInfos[ index ].allocationSize, 0, &stagingData ) );
    
    teMemcpy( stagingData, src, imageSize );

    VkMappedMemoryRange flushRange = {};
    flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    flushRange.memory = renderer.textureStagingMemories[ index ];
    flushRange.size = VK_WHOLE_SIZE;
    vkFlushMappedMemoryRanges( renderer.device, 1, &flushRange );

    vkUnmapMemory( renderer.device, renderer.textureStagingMemories[ index ] );
}

static VkPipeline CreatePipeline( const teShader& shader, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teFillMode fillMode, teTopology topology, teTextureFormat colorFormat, teTextureFormat depthFormat, bool isUI )
{
    VkVertexInputAttributeDescription attribute_desc[ 3 ] = {};
    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    
    VkVertexInputBindingDescription bindingDesc[ 1 ] = {};
    bindingDesc[ 0 ].stride = 4 * 4 + 4; // sizeof( ImDrawVert );
    bindingDesc[ 0 ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    if (isUI)
    {
        attribute_desc[ 0 ].location = 0;
        attribute_desc[ 0 ].binding = 0;
        attribute_desc[ 0 ].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[ 0 ].offset = 0;
        attribute_desc[ 1 ].location = 1;
        attribute_desc[ 1 ].binding = 0;
        attribute_desc[ 1 ].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[ 1 ].offset = 2 * 4;
        attribute_desc[ 2 ].location = 2;
        attribute_desc[ 2 ].binding = 0;
        attribute_desc[ 2 ].format = VK_FORMAT_R8G8B8A8_UNORM;
        attribute_desc[ 2 ].offset = 4 * 4;

        vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_info.vertexBindingDescriptionCount = 1;
        vertex_info.pVertexBindingDescriptions = bindingDesc;
        vertex_info.vertexAttributeDescriptionCount = 3;
        vertex_info.pVertexAttributeDescriptions = attribute_desc;
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = topology == teTopology::Triangles ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = fillMode == teFillMode::Solid ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE;

    if (cullMode == teCullMode::Off)
    {
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
    }
    else if (cullMode == teCullMode::CCW)
    {
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    }
    else
    {
        teAssert( !"unhandled cull mode" );
    }

    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1;

    VkPipelineColorBlendAttachmentState blendAttachmentState[ 1 ] = {};
    blendAttachmentState[ 0 ].colorWriteMask = 0xF;
    blendAttachmentState[ 0 ].blendEnable = blendMode != teBlendMode::Off ? VK_TRUE : VK_FALSE;
    blendAttachmentState[ 0 ].alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState[ 0 ].colorBlendOp = VK_BLEND_OP_ADD;

    if (blendMode == teBlendMode::Alpha)
    {
        blendAttachmentState[ 0 ].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState[ 0 ].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState[ 0 ].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState[ 0 ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    }
    else if (blendMode == teBlendMode::Additive)
    {
        blendAttachmentState[ 0 ].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState[ 0 ].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState[ 0 ].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState[ 0 ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    VkDynamicState dynamicStateEnables[ 2 ] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = &dynamicStateEnables[ 0 ];
    dynamicState.dynamicStateCount = 2;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = depthMode == teDepthMode::NoneWriteOff ? VK_FALSE : VK_TRUE;
    depthStencilState.depthWriteEnable = depthMode == teDepthMode::LessOrEqualWriteOn ? VK_TRUE : VK_FALSE;

    if (depthMode == teDepthMode::LessOrEqualWriteOn || depthMode == teDepthMode::LessOrEqualWriteOff)
    {
        depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    }
    else if (depthMode == teDepthMode::NoneWriteOff)
    {
        depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    }
    else
    {
        teAssert( !"unhandled depth function" );
    }

    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineShaderStageCreateInfo vertexInfo, fragmentInfo;
    teShaderGetInfo( shader, vertexInfo, fragmentInfo );
    const VkPipelineShaderStageCreateInfo shaderStages[ 2 ] = { vertexInfo, fragmentInfo };

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineVertexInputStateCreateInfo inputState = {};
    inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkFormat colorFormatVulkan = VK_FORMAT_UNDEFINED;
    VkFormat depthFormatVulkan = VK_FORMAT_UNDEFINED;
    unsigned bpp = 0;
    GetFormatAndBPP( colorFormat, colorFormatVulkan, bpp );
    GetFormatAndBPP( depthFormat, depthFormatVulkan, bpp );

    VkPipelineRenderingCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    info.colorAttachmentCount = 1;
    info.pColorAttachmentFormats = &colorFormatVulkan;
    info.depthAttachmentFormat = depthFormatVulkan;
    
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = renderer.pipelineLayout;
    pipelineCreateInfo.pVertexInputState = &inputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.pNext = &info;
    
    if (isUI)
    {
        pipelineCreateInfo.pVertexInputState = &vertex_info;
    }

    VkPipeline pso;

    VK_CHECK( vkCreateGraphicsPipelines( renderer.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pso ) );
    return pso;
}

void ClearPSOCache()
{
    for (unsigned i = 0; i < Renderer::MaxPSOs; ++i)
    {
        renderer.psos[ i ] = {};
    }

    renderer.boundPSO = VK_NULL_HANDLE;
}

static int GetPSO( const teShader& shader, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teFillMode fillMode, teTopology topology, teTextureFormat colorFormat, teTextureFormat depthFormat, bool isUI )
{
    int psoIndex = -1;

    VkPipelineShaderStageCreateInfo vertexInfo, fragmentInfo;
    teShaderGetInfo( shader, vertexInfo, fragmentInfo );

    for (unsigned i = 0; i < Renderer::MaxPSOs; ++i)
    {
        if (renderer.psos[ i ].blendMode == blendMode && renderer.psos[i].depthMode == depthMode && renderer.psos[ i ].cullMode == cullMode &&
            renderer.psos[ i ].topology == topology && renderer.psos[ i ].fillMode == fillMode &&
            renderer.psos[ i ].isUI == isUI &&
            renderer.psos[ i ].colorFormat == colorFormat && renderer.psos[ i ].depthFormat == depthFormat &&
            renderer.psos[ i ].vertexModule == vertexInfo.module && renderer.psos[ i ].fragmentModule == fragmentInfo.module)
        {
            psoIndex = i;
            break;
        }
    }

    if (psoIndex == -1)
    {
        int nextFreePsoIndex = -1;

        for (unsigned i = 0; i < Renderer::MaxPSOs; ++i)
        {
            if (renderer.psos[ i ].pso == VK_NULL_HANDLE)
            {
                nextFreePsoIndex = i;
                break;
            }
        }

        teAssert( nextFreePsoIndex != -1 );

        if (nextFreePsoIndex == -1)
        {
            return 0;
        }

        psoIndex = nextFreePsoIndex;
        renderer.psos[ psoIndex ].pso = CreatePipeline( shader, blendMode, cullMode, depthMode, fillMode, topology, colorFormat, depthFormat, isUI );
        renderer.psos[ psoIndex ].blendMode = blendMode;
        renderer.psos[ psoIndex ].fillMode = fillMode;
        renderer.psos[ psoIndex ].topology = topology;
        renderer.psos[ psoIndex ].cullMode = cullMode;
        renderer.psos[ psoIndex ].depthMode = depthMode;
        renderer.psos[ psoIndex ].vertexModule = vertexInfo.module;
        renderer.psos[ psoIndex ].fragmentModule = fragmentInfo.module;
        renderer.psos[ psoIndex ].colorFormat = colorFormat;
        renderer.psos[ psoIndex ].depthFormat = depthFormat;
        renderer.psos[ psoIndex ].isUI = isUI;
    }

    return psoIndex;
}

teShader teCreateShader( const struct teFile& vertexFile, const struct teFile& fragmentFile, const char* vertexName, const char* fragmentName )
{
    return teCreateShader( renderer.device, vertexFile, fragmentFile, vertexName, fragmentName );
}

teShader teCreateComputeShader( const teFile& file, const char* name, unsigned threadsPerThreadgroupX, unsigned threadsPerThreadgroupY )
{
    return teCreateComputeShader( renderer.device, renderer.pipelineLayout, file, name, threadsPerThreadgroupX, threadsPerThreadgroupY );
}

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName )
{
    return teCreateTexture2D( renderer.device, renderer.deviceMemoryProperties, width, height, flags, format, debugName );
}

teTextureCube teCreateTextureCube( unsigned dimension, unsigned flags, teTextureFormat format, const char* debugName )
{
    return teCreateTextureCube( renderer.device, renderer.deviceMemoryProperties, dimension, flags, format, debugName );
}

teTexture2D teLoadTexture( const struct teFile& file, unsigned flags, void* pixels, int pixelsWidth, int pixelsHeight, teTextureFormat pixelsFormat )
{
    teTexture2D outTexture = teLoadTexture( file, flags, renderer.device, renderer.textureStagingBuffers[ 0 ], renderer.deviceMemoryProperties, renderer.graphicsQueue, renderer.swapchainResources[ renderer.currentBuffer ].drawCommandBuffer, renderer.properties,
                                            pixels, pixelsWidth, pixelsHeight, pixelsFormat );
    
    return outTexture;
}

teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags )
{
    teTextureCube outTexture = teLoadTexture( negX, posX, negY, posY, negZ, posZ, flags, renderer.device, renderer.textureStagingBuffers, renderer.deviceMemoryProperties, renderer.graphicsQueue, renderer.swapchainResources[ renderer.currentBuffer ].drawCommandBuffer );

    return outTexture;
}

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name )
{
    if (renderer.SetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = object;
        nameInfo.pObjectName = name;
        renderer.SetDebugUtilsObjectNameEXT( device, &nameInfo );
    }
}

void CreateDepthStencil( uint32_t width, uint32_t height )
{
    const VkFormat depthFormats[ 4 ] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
    const teTextureFormat depthTexFormats[ 4 ] = { teTextureFormat::Depth32F_S8, teTextureFormat::Depth32F_S8, teTextureFormat::Depth32F_S8, teTextureFormat::Depth32F_S8 }; // FIXME: Implement missing formats.
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    int depthIndex = 0;

    for (int i = 0; i < 4; ++i)
    {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties( renderer.physicalDevice, depthFormats[ i ], &formatProps );

        if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) && depthFormat == VK_FORMAT_UNDEFINED)
        {
            depthFormat = depthFormats[ i ];
            depthIndex = i;
        }
    }

    teAssert( depthFormat != VK_FORMAT_UNDEFINED && "undefined depth format!" );

    if (depthFormat != VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        printf( "Warning! Depth format is not a floating-point format, but the renderer uses reverse-z that needs it.\n" );
    }

    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = depthFormat;
    image.extent = { width, height, 1 };
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    for (unsigned i = 0; i < renderer.swapchainImageCount; ++i)
    {
        renderer.swapchainResources[ i ].depthFormat = depthTexFormats[ depthIndex ];

        VK_CHECK( vkCreateImage( renderer.device, &image, nullptr, &renderer.swapchainResources[ i ].depthStencilImage ) );
        SetObjectName( renderer.device, (uint64_t)renderer.swapchainResources[ i ].depthStencilImage, VK_OBJECT_TYPE_IMAGE, "depthstencil" );

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements( renderer.device, renderer.swapchainResources[ i ].depthStencilImage, &memReqs );

        VkMemoryAllocateInfo mem_alloc = {};
        mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_alloc.allocationSize = memReqs.size;
        mem_alloc.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, renderer.deviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
        VK_CHECK( vkAllocateMemory( renderer.device, &mem_alloc, nullptr, &renderer.swapchainResources[ i ].depthStencilMem ) );
        SetObjectName( renderer.device, (uint64_t)renderer.swapchainResources[ i ].depthStencilMem, VK_OBJECT_TYPE_DEVICE_MEMORY, "depthStencil" );

        VK_CHECK( vkBindImageMemory( renderer.device, renderer.swapchainResources[ i ].depthStencilImage, renderer.swapchainResources[ i ].depthStencilMem, 0 ) );
        SetImageLayout( renderer.swapchainResources[ 0 ].drawCommandBuffer, renderer.swapchainResources[ i ].depthStencilImage, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT );

        VkImageViewCreateInfo depthStencilView = {};
        depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthStencilView.format = depthFormat;
        depthStencilView.subresourceRange = {};
        depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        depthStencilView.subresourceRange.baseMipLevel = 0;
        depthStencilView.subresourceRange.levelCount = 1;
        depthStencilView.subresourceRange.baseArrayLayer = 0;
        depthStencilView.subresourceRange.layerCount = 1;
        depthStencilView.image = renderer.swapchainResources[ i ].depthStencilImage;
        VK_CHECK( vkCreateImageView( renderer.device, &depthStencilView, nullptr, &renderer.swapchainResources[ i ].depthStencilView ) );
    }
}

static void LoadFunctionPointers()
{
    renderer.createSwapchainKHR = (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr( renderer.device, "vkCreateSwapchainKHR" );
    renderer.getPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr( renderer.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR" );
    renderer.getPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr( renderer.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR" );
    renderer.getPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr( renderer.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR" );
    renderer.getPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr( renderer.instance, "vkGetPhysicalDeviceSurfaceSupportKHR" );
    renderer.getSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr( renderer.device, "vkGetSwapchainImagesKHR" );
    renderer.acquireNextImageKHR = (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr( renderer.device, "vkAcquireNextImageKHR" );
    renderer.queuePresentKHR = (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr( renderer.device, "vkQueuePresentKHR" );
    renderer.SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr( renderer.instance, "vkSetDebugUtilsObjectNameEXT" );
}

void BeginRegion( VkCommandBuffer cmdbuffer, const char* pMarkerName, float r, float g, float b )
{
    if (renderer.CmdBeginDebugUtilsLabelEXT)
    {
        VkDebugUtilsLabelEXT label = {};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = pMarkerName;
        label.color[ 0 ] = r;
        label.color[ 1 ] = g;
        label.color[ 2 ] = b;
        label.color[ 3 ] = 1;
        renderer.CmdBeginDebugUtilsLabelEXT( cmdbuffer, &label );
    }
}

void EndRegion( VkCommandBuffer cmdBuffer )
{
    if (renderer.CmdEndDebugUtilsLabelEXT)
    {
        renderer.CmdEndDebugUtilsLabelEXT( cmdBuffer );
    }
}

void PushGroupMarker( const char* name )
{
    BeginRegion( renderer.swapchainResources[ renderer.currentBuffer ].drawCommandBuffer, name, 0, 1, 0 );
}

void PopGroupMarker()
{
    EndRegion( renderer.swapchainResources[ renderer.currentBuffer ].drawCommandBuffer );
}

void CreateInstance()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "TheseusEngine";
    appInfo.pEngineName = "TheseusEngine";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
    VkExtensionProperties* extensions = (VkExtensionProperties* )TE_ALLOCA( extensionCount * sizeof( VkExtensionProperties ) );
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions );

    bool hasDebugUtils = false;

#if _DEBUG
    for (uint32_t i = 0; i < extensionCount; ++i)
    {
        hasDebugUtils |= teStrcmp( extensions[ i ].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0;
    }
#endif

    const char* enabledExtensions[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
 #if VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif VK_USE_PLATFORM_WAYLAND_KHR
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = hasDebugUtils ? 3 : 2;
    instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
#if _DEBUG
    const char* validationLayerNames[] = { "VK_LAYER_KHRONOS_validation" };

    if (hasDebugUtils)
    {
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = validationLayerNames;
    }
#endif

    VkResult result = vkCreateInstance( &instanceCreateInfo, nullptr, &renderer.instance );

    if (result != VK_SUCCESS)
    {
        printf( "Unable to create instance!\n" );
        return;
    }

#if _DEBUG
    if (hasDebugUtils)
    {
        PFN_vkCreateDebugUtilsMessengerEXT dbgCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( renderer.instance, "vkCreateDebugUtilsMessengerEXT" );
        VkDebugUtilsMessengerCreateInfoEXT dbg_messenger_create_info = {};
        dbg_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbg_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg_messenger_create_info.pfnUserCallback = dbgFunc;
        result = dbgCreateDebugUtilsMessenger( renderer.instance, &dbg_messenger_create_info, nullptr, &renderer.dbgMessenger );

        renderer.CmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr( renderer.instance, "vkCmdBeginDebugUtilsLabelEXT" );
        renderer.CmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr( renderer.instance, "vkCmdEndDebugUtilsLabelEXT" );
    }
#endif
}

void CreateDevice()
{
    uint32_t gpuCount;
    VK_CHECK( vkEnumeratePhysicalDevices( renderer.instance, &gpuCount, nullptr ) );
    VkPhysicalDevice* physicalDevices = (VkPhysicalDevice*)teMalloc( sizeof( VkPhysicalDevice ) * gpuCount );
    VK_CHECK( vkEnumeratePhysicalDevices( renderer.instance, &gpuCount, physicalDevices ) );
    renderer.physicalDevice = physicalDevices[ 0 ];
    teFree( physicalDevices );

    vkGetPhysicalDeviceProperties( renderer.physicalDevice, &renderer.properties );
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties( renderer.physicalDevice, &queueCount, nullptr );

    printf( "GPU: %s\n", renderer.properties.deviceName );

    uint32_t deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties( renderer.physicalDevice, nullptr, &deviceExtensionCount, nullptr );

    VkExtensionProperties* availableExtensions = (VkExtensionProperties*)TE_ALLOCA( deviceExtensionCount * sizeof( VkExtensionProperties ) );
    
    for (uint32_t i = 0; i < deviceExtensionCount; ++i)
    {
        if (teStrstr( availableExtensions[ i ].extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME ))
        {
            renderer.meshShaderSupported = true;
        }
    }

    VkQueueFamilyProperties* queueProps = (VkQueueFamilyProperties*)teMalloc( sizeof( VkQueueFamilyProperties ) * queueCount );
    vkGetPhysicalDeviceQueueFamilyProperties( renderer.physicalDevice, &queueCount, queueProps );

    for (renderer.graphicsQueueIndex = 0; renderer.graphicsQueueIndex < queueCount; ++renderer.graphicsQueueIndex)
    {
        if (queueProps[ renderer.graphicsQueueIndex ].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            break;
        }
    }
    
    free( queueProps );

    float queuePriorities = 0;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = renderer.graphicsQueueIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriorities;

    const char* enabledExtensions[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME
    };
    
    vkGetPhysicalDeviceFeatures( renderer.physicalDevice, &renderer.features );

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
    dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeature.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &dynamicRenderingFeature;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &renderer.features;
    deviceCreateInfo.enabledExtensionCount = renderer.meshShaderSupported ? 2 : 1;
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
    VK_CHECK( vkCreateDevice( renderer.physicalDevice, &deviceCreateInfo, nullptr, &renderer.device ) );

    vkGetPhysicalDeviceMemoryProperties( renderer.physicalDevice, &renderer.deviceMemoryProperties );
    vkGetDeviceQueue( renderer.device, renderer.graphicsQueueIndex, 0, &renderer.graphicsQueue );
}

void CreateCommandBuffers()
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = renderer.graphicsQueueIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK( vkCreateCommandPool( renderer.device, &cmdPoolInfo, nullptr, &renderer.cmdPool ) );

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = renderer.cmdPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 4;

    VkCommandBuffer drawCmdBuffers[ 4 ];
    VK_CHECK( vkAllocateCommandBuffers( renderer.device, &commandBufferAllocateInfo, drawCmdBuffers ) );

    for (uint32_t i = 0; i < 4; ++i)
    {
        renderer.swapchainResources[ i ].drawCommandBuffer = drawCmdBuffers[ i ];
        const char* name = "drawCommandBuffer 0";
        if (i == 1)
        {
            name = "drawCommandBuffer 1";
        }
        else if (i == 2)
        {
            name = "drawCommandBuffer 2";
        }
        else if (i == 3)
        {
            name = "drawCommandBuffer 3";
        }

        SetObjectName( renderer.device, (uint64_t)drawCmdBuffers[ i ], VK_OBJECT_TYPE_COMMAND_BUFFER, name );

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK( vkCreateSemaphore( renderer.device, &semaphoreCreateInfo, nullptr, &renderer.swapchainResources[ i ].imageAcquiredSemaphore ) );
        SetObjectName( renderer.device, (uint64_t)renderer.swapchainResources[ i ].imageAcquiredSemaphore, VK_OBJECT_TYPE_SEMAPHORE, "imageAcquiredSemaphore" );

        VK_CHECK( vkCreateSemaphore( renderer.device, &semaphoreCreateInfo, nullptr, &renderer.swapchainResources[ i ].renderCompleteSemaphore ) );
        SetObjectName( renderer.device, (uint64_t)renderer.swapchainResources[ i ].renderCompleteSemaphore, VK_OBJECT_TYPE_SEMAPHORE, "renderCompleteSemaphore" );

        VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };

        VK_CHECK( vkCreateFence( renderer.device, &fenceCreateInfo, nullptr, &renderer.swapchainResources[ i ].fence ) );
    }
}

void CreateSwapchain( void* windowHandle, unsigned width, unsigned height, unsigned presentInterval )
{
#if VK_USE_PLATFORM_WIN32_KHR
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hinstance = GetModuleHandle( nullptr );
    surfaceCreateInfo.hwnd = (HWND)windowHandle;
    VK_CHECK( vkCreateWin32SurfaceKHR( renderer.instance, &surfaceCreateInfo, nullptr, &renderer.surface ) );
#else
    VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.surface = gwlSurface;
    surfaceCreateInfo.display = gwlDisplay;
    VK_CHECK( vkCreateWaylandSurfaceKHR( renderer.instance, &surfaceCreateInfo, nullptr, &renderer.surface ) );
#endif

    uint32_t formatCount = 0;
    VK_CHECK( renderer.getPhysicalDeviceSurfaceFormatsKHR( renderer.physicalDevice, renderer.surface, &formatCount, nullptr ) );

    VkSurfaceFormatKHR* surfFormats = (VkSurfaceFormatKHR*)teMalloc( sizeof( VkSurfaceFormatKHR ) * formatCount );
    VK_CHECK( renderer.getPhysicalDeviceSurfaceFormatsKHR( renderer.physicalDevice, renderer.surface, &formatCount, surfFormats ) );

    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    bool foundSRGB = false;

    for (uint32_t formatIndex = 0; formatIndex < formatCount; ++formatIndex)
    {
        if (surfFormats[ formatIndex ].format == VK_FORMAT_B8G8R8A8_SRGB || surfFormats[ formatIndex ].format == VK_FORMAT_R8G8B8A8_SRGB)
        {
            colorFormat = surfFormats[ formatIndex ].format;
            renderer.swapchainResources[ 0 ].colorFormat = surfFormats[ formatIndex ].format == VK_FORMAT_B8G8R8A8_SRGB ? teTextureFormat::BGRA_sRGB : teTextureFormat::RGBA_sRGB;
            foundSRGB = true;
        }
    }

    if (!foundSRGB && formatCount == 1 && surfFormats[ 0 ].format == VK_FORMAT_UNDEFINED)
    {
        colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        renderer.swapchainResources[ 0 ].colorFormat = teTextureFormat::BGRA;
    }
    else if (!foundSRGB)
    {
        colorFormat = surfFormats[ 0 ].format;
    }

    VkSurfaceCapabilitiesKHR surfCaps;
    VK_CHECK( renderer.getPhysicalDeviceSurfaceCapabilitiesKHR( renderer.physicalDevice, renderer.surface, &surfCaps ) );

    const VkExtent2D swapchainExtent = (surfCaps.currentExtent.width == (uint32_t)-1) ? VkExtent2D{ (uint32_t)width, (uint32_t)height } : surfCaps.currentExtent;

    if (surfCaps.currentExtent.width != (uint32_t)-1)
    {
        width = surfCaps.currentExtent.width;
        height = surfCaps.currentExtent.height;
    }

    renderer.swapchainWidth = width;
    renderer.swapchainHeight = height;

    VkSurfaceTransformFlagsKHR preTransform = (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCaps.currentTransform;

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = renderer.surface;
    swapchainInfo.minImageCount = surfCaps.minImageCount;
    swapchainInfo.imageFormat = colorFormat;
    swapchainInfo.imageColorSpace = surfFormats[ 0 ].colorSpace;
    swapchainInfo.imageExtent = { swapchainExtent.width, swapchainExtent.height };
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.presentMode = presentInterval == 0 ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    teFree( surfFormats );

    VK_CHECK( renderer.createSwapchainKHR( renderer.device, &swapchainInfo, nullptr, &renderer.swapchain ) );
    SetObjectName( renderer.device, (uint64_t)renderer.swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "swap chain" );

    VK_CHECK( renderer.getSwapchainImagesKHR( renderer.device, renderer.swapchain, &renderer.swapchainImageCount, nullptr ) );
    VkImage* images = (VkImage*)teMalloc( sizeof( VkImage ) * renderer.swapchainImageCount );
    VK_CHECK( renderer.getSwapchainImagesKHR( renderer.device, renderer.swapchain, &renderer.swapchainImageCount, images ) );

    for (uint32_t i = 0; i < swapchainInfo.minImageCount; ++i)
    {
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.format = colorFormat;
        colorAttachmentView.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.image = images[ i ];
        VK_CHECK( vkCreateImageView( renderer.device, &colorAttachmentView, nullptr, &renderer.swapchainResources[ i ].view ) );
        SetObjectName( renderer.device, (uint64_t)renderer.swapchainResources[ i ].view, VK_OBJECT_TYPE_IMAGE_VIEW, "swapchain view" );

        renderer.swapchainResources[ i ].image = images[ i ];

        SetObjectName( renderer.device, (uint64_t)images[ i ], VK_OBJECT_TYPE_IMAGE, "swapchain image" );
        SetImageLayout( renderer.swapchainResources[ 0 ].drawCommandBuffer, renderer.swapchainResources[ i ].image,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT );

    }

    teFree( images );
}

void CreateDescriptorSets()
{
    VkDescriptorSetLayoutBinding bindings[ DescriptorEntryCount ] = {};
    bindings[ 0 ].binding = 0;
    bindings[ 0 ].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[ 0 ].descriptorCount = TextureCount;
    bindings[ 0 ].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[ 1 ].binding = 1;
    bindings[ 1 ].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[ 1 ].descriptorCount = TextureCount;
    bindings[ 1 ].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[ 2 ].binding = 2;
    bindings[ 2 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    bindings[ 2 ].descriptorCount = 1;
    bindings[ 2 ].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT;

    bindings[ 3 ].binding = 3;
    bindings[ 3 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[ 3 ].descriptorCount = 1;
    bindings[ 3 ].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT;

    bindings[ 4 ].binding = 4;
    bindings[ 4 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    bindings[ 4 ].descriptorCount = 1;
    bindings[ 4 ].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT;

    bindings[ 5 ].binding = 5;
    bindings[ 5 ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[ 5 ].descriptorCount = 1;
    bindings[ 5 ].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[ 6 ].binding = 6;
    bindings[ 6 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    bindings[ 6 ].descriptorCount = 1;
    bindings[ 6 ].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT;

    VkDescriptorSetLayoutCreateInfo setCreateInfo = {};
    setCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setCreateInfo.bindingCount = DescriptorEntryCount;
    setCreateInfo.pBindings = bindings;

    VK_CHECK( vkCreateDescriptorSetLayout( renderer.device, &setCreateInfo, nullptr, &renderer.descriptorSetLayout ) );
    SetObjectName( renderer.device, (uint64_t)renderer.descriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "descriptorSetLayout" );

    VkDescriptorPoolSize typeCounts[ DescriptorEntryCount ];
    typeCounts[ 0 ].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    typeCounts[ 0 ].descriptorCount = renderer.swapchainImageCount * TextureCount * renderer.swapchainResources[ 0 ].SetCount;
    typeCounts[ 1 ].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    typeCounts[ 1 ].descriptorCount = renderer.swapchainImageCount * TextureCount * renderer.swapchainResources[ 0 ].SetCount;
    typeCounts[ 2 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    typeCounts[ 2 ].descriptorCount = renderer.swapchainImageCount * renderer.swapchainResources[ 0 ].SetCount;
    typeCounts[ 3 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[ 3 ].descriptorCount = renderer.swapchainImageCount * renderer.swapchainResources[ 0 ].SetCount;
    typeCounts[ 4 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    typeCounts[ 4 ].descriptorCount = renderer.swapchainImageCount * renderer.swapchainResources[ 0 ].SetCount;
    typeCounts[ 5 ].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typeCounts[ 5 ].descriptorCount = renderer.swapchainImageCount * renderer.swapchainResources[ 0 ].SetCount;
    typeCounts[ 6 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    typeCounts[ 6 ].descriptorCount = renderer.swapchainImageCount * renderer.swapchainResources[ 0 ].SetCount;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = DescriptorEntryCount;
    descriptorPoolInfo.pPoolSizes = typeCounts;
    descriptorPoolInfo.maxSets = renderer.swapchainImageCount * renderer.swapchainResources[ 0 ].SetCount;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK( vkCreateDescriptorPool( renderer.device, &descriptorPoolInfo, nullptr, &renderer.descriptorPool ) );
    SetObjectName( renderer.device, (uint64_t)renderer.descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "descriptorPool" );

    for (uint32_t i = 0; i < renderer.swapchainImageCount; ++i)
    {
        for (uint32_t s = 0; s < renderer.swapchainResources[ 0 ].SetCount; ++s)
        {
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = renderer.descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &renderer.descriptorSetLayout;

            VK_CHECK( vkAllocateDescriptorSets( renderer.device, &allocInfo, &renderer.swapchainResources[ i ].descriptorSets[ s ] ) );
            SetObjectName( renderer.device, (uint64_t)renderer.swapchainResources[ i ].descriptorSets[ s ], VK_OBJECT_TYPE_DESCRIPTOR_SET, "descriptorSets" );
        }
    }
}

void CreateBuffers()
{
    unsigned bufferBytes = 1024 * 1024 * 250;
    renderer.staticMeshPositionBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferViewType::Float3, "staticMeshPositionBuffer" );
    renderer.staticMeshPositionStagingBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Float3, "staticMeshPositionStagingBuffer" );
    renderer.staticMeshUVBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferViewType::Float2, "staticMeshUVBuffer" );
    renderer.staticMeshUVStagingBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Float2, "staticMeshUVStagingBuffer" );
    renderer.staticMeshNormalBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferViewType::Float3, "staticMeshNormalBuffer" );
    renderer.staticMeshNormalStagingBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Float3, "staticMeshNormalStagingBuffer" );
    renderer.staticMeshTangentBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferViewType::Float4, "staticMeshTangentBuffer" );
    renderer.staticMeshTangentStagingBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Float4, "staticMeshTangentStagingBuffer" );
    renderer.staticMeshIndexBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferViewType::Ushort, "staticMeshIndexBuffer" );
    renderer.staticMeshIndexStagingBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, bufferBytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Ushort, "staticMeshIndexStagingBuffer" );
    renderer.uiVertexBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, 1024 * 1024 * 8, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Float3, "uiVertexBuffer" );
    renderer.uiIndexBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, 1024 * 1024 * 8, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferViewType::Ushort, "uiIndexBuffer" );
    renderer.uiVertices = (float*)teMalloc( 1024 * 1024 * 8 );
    renderer.uiIndices = (uint16_t*)teMalloc( 1024 * 1024 * 8 );

    for (unsigned i = 0; i < 4; ++i)
    {
        renderer.swapchainResources[ i ].ubo.buffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, renderer.uboSizeBytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, BufferViewType::Uint, "UBO" );
        VK_CHECK( vkMapMemory( renderer.device, BufferGetMemory( renderer.swapchainResources[ i ].ubo.buffer ), 0, renderer.uboSizeBytes, 0, (void**)&renderer.swapchainResources[ i ].ubo.uboData ) );
    }
}

void teFinalizeMeshBuffers()
{
    CopyBuffer( renderer.staticMeshIndexStagingBuffer, renderer.staticMeshIndexBuffer );
    CopyBuffer( renderer.staticMeshUVStagingBuffer, renderer.staticMeshUVBuffer );
    CopyBuffer( renderer.staticMeshPositionStagingBuffer, renderer.staticMeshPositionBuffer );
    CopyBuffer( renderer.staticMeshNormalStagingBuffer, renderer.staticMeshNormalBuffer );
    CopyBuffer( renderer.staticMeshTangentStagingBuffer, renderer.staticMeshTangentBuffer );
}

void CreateSamplers()
{
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = samplerInfo.magFilter;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.maxAnisotropy = 1;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VK_CHECK( vkCreateSampler( renderer.device, &samplerInfo, nullptr, &renderer.samplerNearestRepeat ) );
    SetObjectName( renderer.device, (uint64_t)renderer.samplerNearestRepeat, VK_OBJECT_TYPE_SAMPLER, "samplerNearestRepeat" );

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    VK_CHECK( vkCreateSampler( renderer.device, &samplerInfo, nullptr, &renderer.samplerNearestClamp ) );
    SetObjectName( renderer.device, (uint64_t)renderer.samplerNearestClamp, VK_OBJECT_TYPE_SAMPLER, "samplerNearestClamp" );

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = samplerInfo.magFilter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    VK_CHECK( vkCreateSampler( renderer.device, &samplerInfo, nullptr, &renderer.samplerLinearRepeat ) );
    SetObjectName( renderer.device, (uint64_t)renderer.samplerLinearRepeat, VK_OBJECT_TYPE_SAMPLER, "samplerLinearRepeat" );

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    VK_CHECK( vkCreateSampler( renderer.device, &samplerInfo, nullptr, &renderer.samplerLinearClamp ) );
    SetObjectName( renderer.device, (uint64_t)renderer.samplerLinearClamp, VK_OBJECT_TYPE_SAMPLER, "samplerLinearClamp" );

    if (renderer.features.samplerAnisotropy == VK_FALSE)
    {
        printf( "Anisotropy is not supported! Anisotropic samplers are not using anisotropy.\n" );
    }

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = samplerInfo.magFilter;
    samplerInfo.maxAnisotropy = renderer.features.samplerAnisotropy == VK_TRUE ? 8.0f : 1.0f;
    samplerInfo.anisotropyEnable = renderer.features.samplerAnisotropy;
    VK_CHECK( vkCreateSampler( renderer.device, &samplerInfo, nullptr, &renderer.samplerAnisotropic8Repeat ) );
    SetObjectName( renderer.device, (uint64_t)renderer.samplerAnisotropic8Repeat, VK_OBJECT_TYPE_SAMPLER, "samplerAnisotropic8Repeat" );

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    VK_CHECK( vkCreateSampler( renderer.device, &samplerInfo, nullptr, &renderer.samplerAnisotropic8Clamp ) );
    SetObjectName( renderer.device, (uint64_t)renderer.samplerAnisotropic8Clamp, VK_OBJECT_TYPE_SAMPLER, "samplerAnisotropic8Clamp" );
}

void CopyVulkanBuffer( VkBuffer source, VkBuffer destination, unsigned bufferSize )
{
    VkCommandBufferAllocateInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufInfo.commandPool = renderer.cmdPool;
    cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufInfo.commandBufferCount = 1;

    // TODO: maybe store this in renderer?
    VkCommandBuffer copyCommandBuffer;
    VK_CHECK( vkAllocateCommandBuffers( renderer.device, &cmdBufInfo, &copyCommandBuffer ) );

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkBufferCopy copyRegion = {};
    copyRegion.size = bufferSize;

    VK_CHECK( vkBeginCommandBuffer( copyCommandBuffer, &cmdBufferBeginInfo ) );

    vkCmdCopyBuffer( copyCommandBuffer, source, destination, 1, &copyRegion );

    VK_CHECK( vkEndCommandBuffer( copyCommandBuffer ) );

    VkSubmitInfo copySubmitInfo = {};
    copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    copySubmitInfo.commandBufferCount = 1;
    copySubmitInfo.pCommandBuffers = &copyCommandBuffer;

    VK_CHECK( vkQueueSubmit( renderer.graphicsQueue, 1, &copySubmitInfo, VK_NULL_HANDLE ) );
    VK_CHECK( vkQueueWaitIdle( renderer.graphicsQueue ) );
    vkFreeCommandBuffers( renderer.device, cmdBufInfo.commandPool, 1, &copyCommandBuffer );
}

void UpdateStagingBuffer( const Buffer& buffer, const void* data, unsigned dataBytes, unsigned offset )
{
    teAssert( BufferGetMemory( buffer ) != VK_NULL_HANDLE );
    // TODO: assert that buffer can be mapped.

    void* bufferData = nullptr;
    VK_CHECK( vkMapMemory( renderer.device, BufferGetMemory( buffer ), offset, dataBytes, 0, &bufferData ) );

    teMemcpy( bufferData, data, dataBytes );
    vkUnmapMemory( renderer.device, BufferGetMemory( buffer ) );
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

void teCreateRenderer( unsigned swapInterval, void* windowHandle, unsigned width, unsigned height )
{
	CreateInstance();
    CreateDevice();
    LoadFunctionPointers();
    CreateCommandBuffers();
    CreateBuffers();
    CreateSamplers();

    SetObjectName( renderer.device, (uint64_t)renderer.instance, VK_OBJECT_TYPE_INSTANCE, "renderer.instance" );
    SetObjectName( renderer.device, (uint64_t)renderer.graphicsQueue, VK_OBJECT_TYPE_QUEUE, "renderer.graphicsQueue" );

    for (unsigned i = 0; i < 6; ++i)
    {
        CreateStagingTexture( i );
    }

    renderer.defaultTexture2D = teCreateTexture2D( 32, 32, 0, teTextureFormat::RGBA_sRGB, "default texture 2D" );
    renderer.defaultTextureCube = teCreateTextureCube( 32, 0, teTextureFormat::RGBA_sRGB, "default texture Cube" );
    renderer.nullUAV = teCreateTexture2D( 16, 16, teTextureFlags::UAV, teTextureFormat::R32F, "nullUAV" );
    renderer.nullBuffer = CreateBuffer( renderer.device, renderer.deviceMemoryProperties, 256, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, BufferViewType::Ushort, "nullBuffer" );;

    for (unsigned i = 0; i < TextureCount; ++i)
    {
        renderer.samplerInfos[ i ].imageView = TextureGetView( renderer.defaultTexture2D );
    }

    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK( vkBeginCommandBuffer( renderer.swapchainResources[ 0 ].drawCommandBuffer, &cmdBufInfo ) );

    CreateSwapchain( windowHandle, width, height, swapInterval );
    CreateDescriptorSets();

    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &renderer.descriptorSetLayout;

    VkPushConstantRange pushConstantRange = {};

    if (renderer.meshShaderSupported)
    {
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    else
    {
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    }
    
    pushConstantRange.size = sizeof( PushConstants );

    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK( vkCreatePipelineLayout( renderer.device, &createInfo, nullptr, &renderer.pipelineLayout ) );

    CreateDepthStencil( renderer.swapchainWidth, renderer.swapchainHeight );

    SetImageLayout( renderer.swapchainResources[ 0 ].drawCommandBuffer, TextureGetImage( renderer.defaultTexture2D ), VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT );

    vkEndCommandBuffer( renderer.swapchainResources[ 0 ].drawCommandBuffer );

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &renderer.swapchainResources[ 0 ].drawCommandBuffer;
    VK_CHECK( vkQueueSubmit( renderer.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );
    VK_CHECK( vkQueueWaitIdle( renderer.graphicsQueue ) );
}

void teBeginFrame()
{
    vkWaitForFences( renderer.device, 1, &renderer.swapchainResources[ renderer.frameIndex ].fence, VK_TRUE, UINT64_MAX );
    vkResetFences( renderer.device, 1, &renderer.swapchainResources[ renderer.frameIndex ].fence );

    VkResult err = renderer.acquireNextImageKHR( renderer.device, renderer.swapchain, UINT64_MAX, renderer.swapchainResources[ renderer.frameIndex ].imageAcquiredSemaphore, VK_NULL_HANDLE, &renderer.currentBuffer );

    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        printf( "Swapchain is out of date!\n" );
    }
    else if (err == VK_TIMEOUT)
    {
        printf( "Swapchain timeout!\n" );
    }
    else if (err == VK_SUBOPTIMAL_KHR)
    {
        printf( "Swapchain is suboptimal!\n" );
    }
    else
    {
        teAssert( err == VK_SUCCESS );
    }

    for (unsigned i = 0; i < TextureCount; ++i)
    {
        renderer.samplerInfos[ i ].imageView = TextureGetView( renderer.defaultTexture2D );
        renderer.samplerInfos[ i ].sampler = renderer.samplerLinearRepeat;
        renderer.samplerInfos[ i ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //renderer.samplerInfosCube[ i ].sampler = renderer.samplerLinearRepeat;
        //renderer.samplerInfosCube[ i ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    renderer.swapchainResources[ renderer.frameIndex ].ubo.offset = 0;
    renderer.boundPSO = VK_NULL_HANDLE;
    renderer.statDrawCalls = 0;
    renderer.statPSOBinds = 0;

    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK( vkBeginCommandBuffer( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, &cmdBufInfo ) );
}

void teEndFrame()
{
    VK_CHECK( vkEndCommandBuffer( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer ) );

    VkPipelineStageFlags pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &pipelineStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &renderer.swapchainResources[ renderer.frameIndex ].imageAcquiredSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderer.swapchainResources[ renderer.frameIndex ].renderCompleteSemaphore;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer;

    VK_CHECK( vkQueueSubmit( renderer.graphicsQueue, 1, &submitInfo, renderer.swapchainResources[ renderer.frameIndex ].fence ) );

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &renderer.swapchain;
    presentInfo.pImageIndices = &renderer.currentBuffer;
    presentInfo.pWaitSemaphores = &renderer.swapchainResources[ renderer.frameIndex ].renderCompleteSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    VkResult err = renderer.queuePresentKHR( renderer.graphicsQueue, &presentInfo );

    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        printf( "Swapchain is out of date!\n" );
    }
    else if (err == VK_SUBOPTIMAL_KHR)
    {
        printf( "Swapchain is suboptimal!\n" );
    }
    else
    {
        teAssert( err == VK_SUCCESS );
    }

    renderer.frameIndex = (renderer.frameIndex + 1) % renderer.swapchainImageCount;

#if VK_USE_PLATFORM_WAYLAND_KHR
    WaylandDispatch();
#endif
}

void BeginRendering( teTexture2D& color, teTexture2D& depth, teClearFlag clearFlag, const float* clearColor )
{
    VkRenderingAttachmentInfo colorAtt{};
    colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView = TextureGetView( color );
    colorAtt.loadOp = clearFlag == teClearFlag::DepthAndColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.clearValue.color.float32[ 0 ] = clearColor[ 0 ];
    colorAtt.clearValue.color.float32[ 1 ] = clearColor[ 1 ];
    colorAtt.clearValue.color.float32[ 2 ] = clearColor[ 2 ];
    colorAtt.clearValue.color.float32[ 3 ] = clearColor[ 3 ];

    VkRenderingAttachmentInfo depthAtt{};
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = TextureGetView( depth );
    depthAtt.loadOp = clearFlag == teClearFlag::DontClear ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.imageLayout = depth.format == teTextureFormat::Depth32F_S8 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAtt.clearValue.depthStencil.depth = 0;

    unsigned width = 0, height = 0;
    teTextureGetDimension( color, width, height );

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.flags = 0;
    renderInfo.layerCount = 1;
    renderInfo.pColorAttachments = &colorAtt;
    renderInfo.pDepthAttachment = &depthAtt;
    renderInfo.renderArea = { { 0, 0 }, { width, height } };

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.image = TextureGetImage( color );
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );

    vkCmdBeginRendering( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, &renderInfo );

    VkViewport viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    vkCmdSetViewport( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, 0, 1, &viewport );

    VkRect2D scissor = { { 0, 0 }, { width, height } };
    vkCmdSetScissor( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, 0, 1, &scissor );

    vkCmdBindIndexBuffer( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, BufferGetBuffer( renderer.staticMeshIndexBuffer ), 0, VK_INDEX_TYPE_UINT16 );

    renderer.currentColorFormat = color.format;
    renderer.currentDepthFormat = depth.format;
}

void EndRendering( teTexture2D& color )
{
    vkCmdEndRendering( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer );

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.image = TextureGetImage( color );
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier );
}

void teBeginSwapchainRendering()
{
    VkRenderingAttachmentInfo colorAtt{};
    colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView = renderer.swapchainResources[ renderer.currentBuffer ].view;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo depthAtt{};
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = renderer.swapchainResources[ renderer.currentBuffer ].depthStencilView;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.flags = 0;
    renderInfo.layerCount = 1;
    renderInfo.pColorAttachments = &colorAtt;
    renderInfo.pDepthAttachment = &depthAtt;
    renderInfo.renderArea = { { 0, 0 }, { renderer.swapchainWidth, renderer.swapchainHeight } };

    vkCmdBeginRendering( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, &renderInfo );

    VkViewport viewport = { 0.0f, 0.0f, (float)renderer.swapchainWidth, (float)renderer.swapchainHeight, 0.0f, 1.0f };
    vkCmdSetViewport( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, 0, 1, &viewport );

    VkRect2D scissor = { { 0, 0 }, { renderer.swapchainWidth, renderer.swapchainHeight } };
    vkCmdSetScissor( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, 0, 1, &scissor );

    vkCmdBindIndexBuffer( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, BufferGetBuffer( renderer.staticMeshIndexBuffer ), 0, VK_INDEX_TYPE_UINT16 );

    PushGroupMarker( "Swap chain" );

    renderer.currentColorFormat = renderer.swapchainResources[ 0 ].colorFormat;
    renderer.currentDepthFormat = renderer.swapchainResources[ 0 ].depthFormat;
}

void teEndSwapchainRendering()
{
    vkCmdEndRendering( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer );
    PopGroupMarker();
}

void UpdateUBO( const float localToClip[ 16 ], const float localToView[ 16 ], const float localToShadowClip[ 16 ], const ShaderParams& shaderParams )
{
    PerObjectUboStruct uboStruct = {};
    uboStruct.localToClip.InitFrom( localToClip );
    uboStruct.localToView.InitFrom( localToView );
    uboStruct.localToShadowClip.InitFrom( localToShadowClip );
    uboStruct.bloomParams.w = shaderParams.bloomThreshold;
    uboStruct.tilesXY.x = shaderParams.tilesXY[ 0 ];
    uboStruct.tilesXY.y = shaderParams.tilesXY[ 1 ];
    uboStruct.tilesXY.z = shaderParams.tilesXY[ 2 ];
    uboStruct.tilesXY.w = shaderParams.tilesXY[ 3 ];

    teMemcpy( renderer.swapchainResources[ renderer.frameIndex ].ubo.uboData + renderer.swapchainResources[ renderer.frameIndex ].ubo.offset, &uboStruct, sizeof( uboStruct ) );
}

static void UpdateDescriptors( const Buffer& binding2, const Buffer& binding4, const Buffer& binding6, const teTexture2D& writeTexture, size_t uboOffset )
{
    const VkDescriptorSet& dstSet = renderer.swapchainResources[ renderer.frameIndex ].descriptorSets[ renderer.swapchainResources[ renderer.frameIndex ].setIndex ];

    VkWriteDescriptorSet sets[ DescriptorEntryCount ] = {};
    sets[ 0 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 0 ].dstSet = dstSet;
    sets[ 0 ].descriptorCount = TextureCount;
    sets[ 0 ].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sets[ 0 ].pImageInfo = &renderer.samplerInfos[ 0 ];
    sets[ 0 ].dstBinding = 0;

    sets[ 1 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 1 ].dstSet = dstSet;
    sets[ 1 ].descriptorCount = TextureCount;
    sets[ 1 ].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    sets[ 1 ].pImageInfo = &renderer.samplerInfos[ 0 ];
    sets[ 1 ].dstBinding = 1;

    VkBufferView binding2View = BufferGetView( binding2 );

    sets[ 2 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 2 ].dstSet = dstSet;
    sets[ 2 ].descriptorCount = 1;
    sets[ 2 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    sets[ 2 ].pTexelBufferView = &binding2View;
    sets[ 2 ].dstBinding = 2;

    VkDescriptorBufferInfo uboDesc = {};
    uboDesc.buffer = BufferGetBuffer( renderer.swapchainResources[ renderer.frameIndex ].ubo.buffer );
    uboDesc.offset = uboOffset;
    uboDesc.range = sizeof( PerObjectUboStruct );

    sets[ 3 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 3 ].dstSet = dstSet;
    sets[ 3 ].descriptorCount = 1;
    sets[ 3 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sets[ 3 ].pBufferInfo = &uboDesc;
    sets[ 3 ].dstBinding = 3;

    VkBufferView binding4View = BufferGetView( binding4 );

    sets[ 4 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 4 ].dstSet = dstSet;
    sets[ 4 ].descriptorCount = 1;
    sets[ 4 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    sets[ 4 ].pTexelBufferView = &binding4View;
    sets[ 4 ].dstBinding = 4;

    VkDescriptorImageInfo rwTexture2dDesc = {};
    rwTexture2dDesc.imageView = TextureGetView( writeTexture );
    rwTexture2dDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    sets[ 5 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 5 ].dstSet = dstSet;
    sets[ 5 ].descriptorCount = 1;
    sets[ 5 ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sets[ 5 ].pImageInfo = &rwTexture2dDesc;
    sets[ 5 ].dstBinding = 5;

    VkBufferView binding6View = BufferGetView( binding6 );

    sets[ 6 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sets[ 6 ].dstSet = dstSet;
    sets[ 6 ].descriptorCount = 1;
    sets[ 6 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    sets[ 6 ].pTexelBufferView = &binding6View;
    sets[ 6 ].dstBinding = 6;

    vkUpdateDescriptorSets( renderer.device, DescriptorEntryCount, sets, 0, nullptr );
}

static void BindDescriptors( VkPipelineBindPoint bindPoint )
{
    vkCmdBindDescriptorSets( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, bindPoint,
                             renderer.pipelineLayout, 0, 1, &renderer.swapchainResources[ renderer.frameIndex ].descriptorSets[ renderer.swapchainResources[ renderer.frameIndex ].setIndex ], 0, nullptr );

    renderer.swapchainResources[ renderer.frameIndex ].setIndex = (renderer.swapchainResources[ renderer.frameIndex ].setIndex + 1) % renderer.swapchainResources[ renderer.frameIndex ].SetCount;
}

void MoveToNextUboOffset()
{
    constexpr size_t offset = sizeof( PerObjectUboStruct );
    const size_t offsetAligned = (offset + renderer.properties.limits.minUniformBufferOffsetAlignment - 1) & ~(renderer.properties.limits.minUniformBufferOffsetAlignment - 1);
    renderer.swapchainResources[ renderer.frameIndex ].ubo.offset += offsetAligned;
    teAssert( renderer.swapchainResources[ renderer.frameIndex ].ubo.offset < renderer.uboSizeBytes );
}

void teShaderDispatch( const teShader& shader, unsigned groupsX, unsigned groupsY, unsigned groupsZ, const ShaderParams& params, const char* debugName )
{
    renderer.shaderParams = params;

    Matrix identity;
    UpdateUBO( identity.m, identity.m, identity.m, params );

    BeginRegion( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, debugName, 1, 1, 1 );

    if (params.writeTexture != 0)
    {
        teTexture2D tex;
        tex.index = params.writeTexture;
        
        SetImageLayout( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, TextureGetImage( tex ), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 0, 1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );
    }

    int textureIndex = 0;

    if (params.readTexture != 0)
    {
        teTexture2D tex;
        tex.index = params.readTexture;
        textureIndex = (int)params.readTexture;

        renderer.samplerInfos[ textureIndex ].imageView = TextureGetView( tex );
    }

    teTexture2D uav;
    uav.index = (params.writeTexture != 0) ? params.writeTexture : renderer.nullUAV.index;

    UpdateDescriptors( renderer.nullBuffer, renderer.nullBuffer, renderer.nullBuffer, uav, renderer.swapchainResources[ renderer.frameIndex ].ubo.offset );

    BindDescriptors( VK_PIPELINE_BIND_POINT_COMPUTE );

    int pushConstants[ 5 ] = { textureIndex, textureIndex, textureIndex, 0, 0 };
    if (renderer.meshShaderSupported)
    {
        vkCmdPushConstants( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, renderer.pipelineLayout, VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
    }
    else
    {
        vkCmdPushConstants( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, renderer.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
    }
    
    vkCmdBindPipeline( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderGetComputePSO( shader ) );
    vkCmdDispatch( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, groupsX, groupsY, groupsZ );

    EndRegion( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer );

    MoveToNextUboOffset();

    teAssert( renderer.swapchainResources[ renderer.frameIndex ].ubo.offset < renderer.uboSizeBytes );

    if (params.writeTexture != 0)
    {
        renderer.samplerInfos[ params.writeTexture ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        teTexture2D tex;
        tex.index = params.writeTexture;

        SetImageLayout( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, TextureGetImage( tex ), VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
    }

    if (params.readTexture != 0)
    {
        teTexture2D tex;
        tex.index = params.readTexture;
        textureIndex = (int)params.readTexture;
    }
}

static VkSampler GetSampler( teTextureSampler sampler )
{
    if (sampler == teTextureSampler::LinearRepeat) return renderer.samplerLinearRepeat;
    if (sampler == teTextureSampler::LinearClamp) return renderer.samplerLinearClamp;
    if (sampler == teTextureSampler::NearestClamp) return renderer.samplerNearestClamp;
    if (sampler == teTextureSampler::NearestRepeat) return renderer.samplerNearestRepeat;
    if (sampler == teTextureSampler::Anisotropic8Clamp) return renderer.samplerAnisotropic8Clamp;
    if (sampler == teTextureSampler::Anisotropic8Repeat) return renderer.samplerAnisotropic8Repeat;
    teAssert( !"Unhandled sampler!" );
    return renderer.samplerLinearRepeat;
}

void Draw( const teShader& shader, unsigned positionOffset, unsigned /*uvOffset*/, unsigned /*normalOffset*/, unsigned indexCount, unsigned indexOffset, teBlendMode blendMode, teCullMode cullMode, teDepthMode depthMode, teTopology topology, teFillMode fillMode,
           unsigned textureIndex, teTextureSampler sampler, unsigned shadowMapIndex )
{
    if (textureIndex != 0)
    {
        teTexture2D tex;
        tex.index = textureIndex;

        for (unsigned i = 0; i < TextureCount; ++i)
        {
            renderer.samplerInfos[ i ].imageView = TextureGetView( tex );
        }

        renderer.samplerInfos[ textureIndex ].sampler = GetSampler( sampler );
    }

    if (shadowMapIndex != 0)
    {
        teTexture2D tex;
        tex.index = shadowMapIndex;

        renderer.samplerInfos[ shadowMapIndex ].imageView = TextureGetView( tex );
        //renderer.samplerInfos[ shadowMapIndex ].sampler = GetSampler( sampler ); // FIXME: We probably want some hardcoded sampler here, not the one used for drawing.
    }

    teTexture2D nullUAV;
    nullUAV.index = renderer.nullUAV.index;

    UpdateDescriptors( renderer.staticMeshPositionBuffer, renderer.staticMeshUVBuffer, renderer.staticMeshNormalBuffer, nullUAV, (unsigned)renderer.swapchainResources[ renderer.frameIndex ].ubo.offset );
    BindDescriptors( VK_PIPELINE_BIND_POINT_GRAPHICS );

    const VkPipeline pso = renderer.psos[ GetPSO( shader, blendMode, cullMode, depthMode, fillMode, topology, renderer.currentColorFormat, renderer.currentDepthFormat, false ) ].pso;

    if (renderer.boundPSO != pso)
    {
        renderer.boundPSO = pso;
        vkCmdBindPipeline( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso );
        ++renderer.statPSOBinds;
    }

    PushConstants pushConstants{};
    pushConstants.textureIndex = (int)textureIndex;
    pushConstants.shadowTextureIndex = (int)shadowMapIndex;

    if (renderer.meshShaderSupported)
    {
        vkCmdPushConstants( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, renderer.pipelineLayout, VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
    }
    else
    {
        vkCmdPushConstants( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, renderer.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
    }
    
    vkCmdDrawIndexed( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, indexCount * 3, 1, indexOffset / 2, positionOffset / (3 * 4), 0 );

    MoveToNextUboOffset();

    ++renderer.statDrawCalls;
}

void teDrawFullscreenTriangle( teShader& shader, teTexture2D& texture, const ShaderParams& shaderParams, teBlendMode blendMode )
{
    Matrix identity;
    UpdateUBO( identity.m, identity.m, identity.m, shaderParams );
    Draw( shader, 0, 0, 0, 3, 0, blendMode, teCullMode::Off, teDepthMode::NoneWriteOff, teTopology::Triangles, teFillMode::Solid, texture.index, teTextureSampler::NearestRepeat, 0 );
}

void teMapUiMemory( void** outVertexMemory, void** outIndexMemory )
{
    *outVertexMemory = renderer.uiVertices;
    *outIndexMemory = renderer.uiIndices;
}

void teUnmapUiMemory()
{
    UpdateStagingBuffer( renderer.uiVertexBuffer, renderer.uiVertices, 8 * 1024 * 1024, 0 );
    UpdateStagingBuffer( renderer.uiIndexBuffer, renderer.uiIndices, 8 * 1024 * 1024, 0 );
}

void teUIDrawCall( const teShader& shader, const teTexture2D& fontTex, int displaySizeX, int displaySizeY, int scissorX, int scissorY, unsigned scissorW, unsigned scissorH, unsigned elementCount, unsigned indexOffset, unsigned vertexOffset )
{
    for (unsigned i = 0; i < TextureCount; ++i)
    {
        renderer.samplerInfos[ i ].imageView = TextureGetView( fontTex );
    }

    renderer.samplerInfos[ fontTex.index ].sampler = renderer.samplerNearestRepeat;

    VkRect2D scissor;
    scissor.offset.x = scissorX;
    scissor.offset.y = scissorY;
    scissor.extent.width = scissorW;
    scissor.extent.height = scissorH;
    vkCmdSetScissor( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, 0, 1, &scissor );

    vkCmdBindIndexBuffer( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, BufferGetBuffer( renderer.uiIndexBuffer ), 0, VK_INDEX_TYPE_UINT16 );

    teTexture2D nullUAV;
    nullUAV.index = (renderer.shaderParams.writeTexture != 0) ? renderer.shaderParams.writeTexture : renderer.nullUAV.index;

    UpdateDescriptors( renderer.uiVertexBuffer, renderer.uiVertexBuffer, renderer.uiVertexBuffer, nullUAV, (unsigned)renderer.swapchainResources[ renderer.frameIndex ].ubo.offset );
    BindDescriptors( VK_PIPELINE_BIND_POINT_GRAPHICS );

    VkDeviceSize offsets[ 1 ] = { 0 };
    VkBuffer buffer = BufferGetBuffer( renderer.uiVertexBuffer );
    vkCmdBindVertexBuffers( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, 0, 1, &buffer, offsets );

    const VkPipeline pso = renderer.psos[ GetPSO( shader, teBlendMode::Alpha, teCullMode::Off, teDepthMode::NoneWriteOff, teFillMode::Solid, teTopology::Triangles, renderer.currentColorFormat, renderer.currentDepthFormat, true ) ].pso;

    if (renderer.boundPSO != pso)
    {
        renderer.boundPSO = pso;
        vkCmdBindPipeline( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso );
        ++renderer.statPSOBinds;
    }

    float displayPosX = 0; // TODO: Get from the application drawData
    float displayPosY = 0;

    PushConstants pushConstants{};
    pushConstants.scale[ 0 ] = 2.0f / displaySizeX;
    pushConstants.scale[ 1 ] = 2.0f / displaySizeY;
    pushConstants.translate[ 0 ] = -1.0f - displayPosX * pushConstants.scale[ 0 ];
    pushConstants.translate[ 1 ] = -1.0f - displayPosY * pushConstants.scale[ 1 ];

    if (renderer.meshShaderSupported)
    {
        vkCmdPushConstants( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, renderer.pipelineLayout, VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
    }
    else
    {
        vkCmdPushConstants( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, renderer.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
    }
    
    vkCmdDrawIndexed( renderer.swapchainResources[ renderer.frameIndex ].drawCommandBuffer, elementCount, 1, indexOffset, vertexOffset, 0 );

    ++renderer.statDrawCalls;
}

float teRendererGetStat( teStat stat )
{
    if (stat == teStat::DrawCalls) return (float)renderer.statDrawCalls;
    if (stat == teStat::PSOBinds) return (float)renderer.statPSOBinds;
    

    return 0;
}

void teLoadMetalShaderLibrary()
{

}
