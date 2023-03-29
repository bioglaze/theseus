#include "texture.h"
#include "file.h"
#include <vulkan/vulkan.h>

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties );
void LoadTGA( const teFile& file, unsigned& outWidth, unsigned& outHeight, unsigned& outDataBeginOffset, unsigned& outBitsPerPixel, unsigned char** outPixelData );
void teMemcpy( void* dst, const void* src, size_t size );
void UpdateStagingTexture( const teFile& file, unsigned width, unsigned height, unsigned dataBeginOffset, unsigned bytesPerPixel );
void SetImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout, unsigned layerCount, unsigned mipLevel, unsigned mipLevelCount, VkPipelineStageFlags srcStageFlags );

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
unsigned textureCount = 1;

static inline unsigned Max2( unsigned x, unsigned y ) noexcept
{
    return x > y ? x : y;
}

static unsigned GetMipLevelCount( unsigned width, unsigned height ) noexcept
{
    unsigned val = width > height ? width : height;
    unsigned count = 1;

    while (val > 1)
    {
        val /= 2;
        ++count;
    }

    return count;
}

void GetFormatAndBPP( teTextureFormat format, bool isSRGB, VkFormat& outFormat, unsigned& outBytesPerPixel )
{
    outFormat = isSRGB ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    outBytesPerPixel = 4;

    if (format == teTextureFormat::BC2)
    {
        outFormat = isSRGB ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (format == teTextureFormat::BC3)
    {
        outFormat = isSRGB ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (format == teTextureFormat::BC4U)
    {
        outFormat = VK_FORMAT_BC4_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (format == teTextureFormat::BC4S)
    {
        outFormat = VK_FORMAT_BC4_SNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (format == teTextureFormat::BC5U)
    {
        outFormat = VK_FORMAT_BC5_UNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (format == teTextureFormat::BC5S)
    {
        outFormat = VK_FORMAT_BC5_SNORM_BLOCK;
        outBytesPerPixel = 2;
    }
    else if (format == teTextureFormat::R32G32F)
    {
        outFormat = VK_FORMAT_R32G32_SFLOAT;
        outBytesPerPixel = 8;
    }
    else if (format == teTextureFormat::R32F)
    {
        outFormat = VK_FORMAT_R32_SFLOAT;
        outBytesPerPixel = 4;
    }
    else if (format == teTextureFormat::Depth32F)
    {
        outFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        outBytesPerPixel = 4;
    }
    else if (format == teTextureFormat::BGRA_sRGB)
    {
        outFormat = VK_FORMAT_B8G8R8A8_SRGB;
        outBytesPerPixel = 4;
    }
    else if (format == teTextureFormat::RGBA_sRGB)
    {
        outFormat = VK_FORMAT_R8G8B8A8_SRGB;
        outBytesPerPixel = 4;
    }
    else if (format == teTextureFormat::BGRA)
    {
        outFormat = VK_FORMAT_B8G8R8A8_UNORM;
        outBytesPerPixel = 4;
    }
}

VkImageView TextureGetView( teTexture2D texture )
{
    return textures[ texture.index ].view;
}

VkImage TextureGetImage( teTexture2D texture )
{
    return textures[ texture.index ].image;
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
    outTex.format = format;

    return outTex;
}

static void CreateBaseMip( teTextureImpl& tex, VkDevice device, VkPhysicalDeviceMemoryProperties deviceMemoryProperties, VkQueue graphicsQueue, VkBuffer stagingBuffer, VkFormat format, unsigned mipLevelCount, const char* debugName, VkCommandBuffer cmdBuffer )
{
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevelCount;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { (uint32_t)tex.width, (uint32_t)tex.height, 1 };
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VK_CHECK( vkCreateImage( device, &imageCreateInfo, nullptr, &tex.image ) );
    SetObjectName( device, (uint64_t)tex.image, VK_OBJECT_TYPE_IMAGE, debugName );

    VkMemoryRequirements memReqs = {};
    vkGetImageMemoryRequirements( device, tex.image, &memReqs );

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    VK_CHECK( vkAllocateMemory( device, &memAllocInfo, nullptr, &tex.deviceMemory ) );
    VK_CHECK( vkBindImageMemory( device, tex.image, tex.deviceMemory, 0 ) );

    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

    SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT );

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = tex.width;
    bufferCopyRegion.imageExtent.height = tex.height;
    bufferCopyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage( cmdBuffer, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion );

    vkEndCommandBuffer( cmdBuffer );

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    VK_CHECK( vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

    vkDeviceWaitIdle( device );
}

static void CreateMipLevels( teTextureImpl& tex, unsigned mipLevelCount, VkDevice device, VkQueue graphicsQueue, VkCommandBuffer cmdBuffer )
{
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

    SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT );

    for (unsigned i = 1; i < mipLevelCount; ++i)
    {
        const int32_t mipWidth = Max2( tex.width >> i, 1 );
        const int32_t mipHeight = Max2( tex.height >> i, 1 );

        VkImageBlit imageBlit = {};
        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.srcSubresource.mipLevel = 0;
        imageBlit.srcOffsets[ 0 ] = { 0, 0, 0 };
        imageBlit.srcOffsets[ 1 ] = { (int32_t)tex.width, (int32_t)tex.height, 1 };

        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.dstSubresource.baseArrayLayer = 0;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.dstOffsets[ 0 ] = { 0, 0, 0 };
        imageBlit.dstOffsets[ 1 ] = { mipWidth, mipHeight, 1 };

        SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, i, 1, VK_PIPELINE_STAGE_TRANSFER_BIT );
        vkCmdBlitImage( cmdBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR );
    }

    vkEndCommandBuffer( cmdBuffer );

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    VK_CHECK( vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

    vkDeviceWaitIdle( device );

    VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

    for (unsigned i = 1; i < mipLevelCount; ++i)
    {
        SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, i, 1, VK_PIPELINE_STAGE_TRANSFER_BIT );
    }

    const int baseMip = mipLevelCount > 1 ? 1 : 0;
    const int mipCount = mipLevelCount > 1 ? (mipLevelCount - 1) : mipLevelCount;
    SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, baseMip, mipCount, VK_PIPELINE_STAGE_TRANSFER_BIT );

    if (mipLevelCount > 1)
    {
        SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT );
    }

    vkEndCommandBuffer( cmdBuffer );

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    VK_CHECK( vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

    vkDeviceWaitIdle( device );
}

teTexture2D teLoadTexture( const struct teFile& file, unsigned flags, VkDevice device, VkBuffer stagingBuffer, VkPhysicalDeviceMemoryProperties deviceMemoryProperties, VkQueue graphicsQueue, VkCommandBuffer cmdBuffer )
{
    teAssert( textureCount < 100 );
    teAssert( !(flags & teTextureFlags::UAV) );

    teTexture2D outTexture;
    outTexture.index = textureCount++;
    teTextureImpl& tex = textures[ outTexture.index ];
    //tex.filter = filter;
    tex.flags = flags;

    unsigned bytesPerPixel = 4;
    unsigned mipLevelCount = 1;
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;

    if (strstr( file.path, ".tga" ) || strstr( file.path, ".TGA" ))
    {
        unsigned bitsPerPixel = 24;
        unsigned dataBeginOffset = 0;
        unsigned char* pixelData = nullptr;

        LoadTGA( file, tex.width, tex.height, dataBeginOffset, bitsPerPixel, &pixelData );
        bytesPerPixel = bitsPerPixel == 24 ? 3 : 4;

        mipLevelCount = (flags & teTextureFlags::GenerateMips) ? GetMipLevelCount( tex.width, tex.height ) : 1;
        teAssert( mipLevelCount <= 15 );

        UpdateStagingTexture( file, tex.width, tex.height, dataBeginOffset, bytesPerPixel );
        CreateBaseMip( tex, device, deviceMemoryProperties, graphicsQueue, stagingBuffer, format, mipLevelCount, file.path, cmdBuffer );
        CreateMipLevels( tex, mipLevelCount, device, graphicsQueue, cmdBuffer );

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.levelCount = mipLevelCount;
        viewInfo.image = tex.image;
        VK_CHECK( vkCreateImageView( device, &viewInfo, nullptr, &tex.view ) );
        SetObjectName( device, (uint64_t)tex.view, VK_OBJECT_TYPE_IMAGE_VIEW, file.path );

        return outTexture;
    }
    else
    {
        teAssert( !"unhandled texture type" );
    }
}
