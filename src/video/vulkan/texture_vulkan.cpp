#include "texture.h"
#include <vulkan/vulkan.h>

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties );

struct teTextureImpl
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VkFormat vulkanFormat = VK_FORMAT_UNDEFINED;
    unsigned flags = 0;
    unsigned width = 0;
    unsigned height = 0;
    unsigned mipLevelCount = 1;
};

teTextureImpl textures[ 100 ];
unsigned textureCount = 0;

static void GetFormatAndBPP( teTextureFormat bcFormat, bool isSRGB, VkFormat& outFormat, unsigned& outBytesPerPixel )
{
    outFormat = isSRGB ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    outBytesPerPixel = 4;

    if (bcFormat == teTextureFormat::BC2)
    {
        outFormat = isSRGB ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (bcFormat == teTextureFormat::BC3)
    {
        outFormat = isSRGB ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (bcFormat == teTextureFormat::BC4U)
    {
        outFormat = VK_FORMAT_BC4_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (bcFormat == teTextureFormat::BC4S)
    {
        outFormat = VK_FORMAT_BC4_SNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (bcFormat == teTextureFormat::BC5U)
    {
        outFormat = VK_FORMAT_BC5_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (bcFormat == teTextureFormat::BC5S)
    {
        outFormat = VK_FORMAT_BC5_SNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (bcFormat == teTextureFormat::R32G32F)
    {
        outFormat = VK_FORMAT_R32G32_SFLOAT;
        outBytesPerPixel = 8;
    }
    else if (bcFormat == teTextureFormat::R32F)
    {
        outFormat = VK_FORMAT_R32_SFLOAT;
        outBytesPerPixel = 4;
    }
    else if (bcFormat == teTextureFormat::Depth32F)
    {
        outFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        outBytesPerPixel = 4;
    }
    else if (bcFormat == teTextureFormat::BGRA_sRGB)
    {
        outFormat = VK_FORMAT_B8G8R8A8_SRGB;
        outBytesPerPixel = 4;
    }
}

VkImageView TextureGetView( teTexture2D texture )
{
    return textures[ texture.index ].view;
}

void teTextureGetDimension( teTexture2D texture, unsigned& outWidth, unsigned& outHeight )
{
    outWidth = textures[ texture.index ].width;
    outHeight = textures[ texture.index ].height;
}

teTexture2D teCreateTexture2D( VkDevice device, VkPhysicalDeviceMemoryProperties deviceMemoryProperties, unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName )
{
    const unsigned index = textureCount++;

    teTextureImpl& tex = textures[ index ];
    tex.width = width;
    tex.height = height;
    tex.flags = flags;

    VkFormat vFormat = VK_FORMAT_UNDEFINED;
    unsigned bpp = 0;
    GetFormatAndBPP( format, false, vFormat, bpp );
    tex.vulkanFormat = vFormat;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = vFormat;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { (uint32_t)tex.width, (uint32_t)tex.height, 1 };
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    if ((flags & teTextureFlags::RenderTexture) && format == teTextureFormat::Depth32F)
    {
        imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else if (flags & teTextureFlags::RenderTexture)
    {
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (flags & teTextureFlags::UAV)
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    VK_CHECK( vkCreateImage( device, &imageCreateInfo, nullptr, &tex.image ) );
    SetObjectName( device, (uint64_t)tex.image, VK_OBJECT_TYPE_IMAGE, debugName );

    VkMemoryRequirements memReqs = {};
    vkGetImageMemoryRequirements( device, tex.image, &memReqs );

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    VK_CHECK( vkAllocateMemory( device, &memAllocInfo, nullptr, &tex.deviceMemory ) );
    SetObjectName( device, (uint64_t)tex.deviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY, debugName );

    VK_CHECK( vkBindImageMemory( device, tex.image, tex.deviceMemory, 0 ) );

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageCreateInfo.format;
    viewInfo.subresourceRange.aspectMask = format == teTextureFormat::Depth32F ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.image = tex.image;
    VK_CHECK( vkCreateImageView( device, &viewInfo, nullptr, &tex.view ) );
    SetObjectName( device, (uint64_t)tex.view, VK_OBJECT_TYPE_IMAGE_VIEW, debugName );

    teTexture2D outTex;
    outTex.index = index;

    return outTex;
}