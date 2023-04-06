#include "texture.h"
#include "file.h"
#include "te_stdlib.h"
#include <vulkan/vulkan.h>

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties );
void LoadTGA( const teFile& file, unsigned& outWidth, unsigned& outHeight, unsigned& outDataBeginOffset, unsigned& outBitsPerPixel, unsigned char** outPixelData );
bool LoadDDS( const teFile& fileContents, unsigned& outWidth, unsigned& outHeight, teTextureFormat& outFormat, unsigned& outMipLevelCount, unsigned( &outMipOffsets )[ 15 ] );
void UpdateStagingTexture( const teFile& file, unsigned width, unsigned height, unsigned dataBeginOffset, unsigned bytesPerPixel, VkFormat format, unsigned index );
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

teTexture2D teCreateTexture2D( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName )
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

static void CreateBaseMip( teTextureImpl& tex, VkDevice device, VkPhysicalDeviceMemoryProperties deviceMemoryProperties, VkQueue graphicsQueue, VkBuffer* stagingBuffers, unsigned stagingBufferCount, VkFormat format, unsigned mipLevelCount, const char* debugName, VkCommandBuffer cmdBuffer )
{
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevelCount;
    imageCreateInfo.arrayLayers = stagingBufferCount;
    imageCreateInfo.flags = stagingBufferCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
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

    SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, stagingBufferCount, 0, 1, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT );

    for (unsigned face = 0; face < stagingBufferCount; ++face)
    {
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = face;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = tex.width;
        bufferCopyRegion.imageExtent.height = tex.height;
        bufferCopyRegion.imageExtent.depth = 1;
    
        vkCmdCopyBufferToImage( cmdBuffer, stagingBuffers[ face ], tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion );
    }

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

static void CopyMipmapsFromDDS( teTextureImpl& tex, bool isOpaque, VkFormat format, const teFile files[ 6 ], unsigned mipOffsets[ 6 ][ 15 ], VkDevice device, VkCommandBuffer cmdBuffer, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties )
{
    for (unsigned face = 0; face < 6; ++face)
    {
        VkBuffer stagingBuffers[ 15 ];
        VkDeviceMemory stagingMemory[ 15 ];

        for (unsigned mipLevel = 1; mipLevel < tex.mipLevelCount; ++mipLevel)
        {
            const int32_t mipWidth = Max2( tex.width >> mipLevel, 1 );
            const int32_t mipHeight = Max2( tex.height >> mipLevel, 1 );

            const VkDeviceSize bc1BlockSize = isOpaque ? 8 : 16;
            VkDeviceSize imageSize = (mipWidth / 4) * (mipHeight / 4) * (format == VK_FORMAT_BC5_UNORM_BLOCK ? 16 : bc1BlockSize);

            if (imageSize == 0)
            {
                imageSize = 16;
            }

            VkBufferCreateInfo bufferCreateInfo = {};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size = imageSize;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            VK_CHECK( vkCreateBuffer( device, &bufferCreateInfo, nullptr, &stagingBuffers[ mipLevel ] ) );

            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements( device, stagingBuffers[ mipLevel ], &memReqs );

            VkMemoryAllocateInfo memAllocInfo = {};
            memAllocInfo.allocationSize = memReqs.size;
            memAllocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
            VK_CHECK( vkAllocateMemory( device, &memAllocInfo, nullptr, &stagingMemory[ mipLevel ] ) );

            VK_CHECK( vkBindBufferMemory( device, stagingBuffers[ mipLevel ], stagingMemory[ mipLevel ], 0 ) );

            void* stagingData;
            VK_CHECK( vkMapMemory( device, stagingMemory[ mipLevel ], 0, memReqs.size, 0, &stagingData ) );

            VkDeviceSize amountToCopy = imageSize;

            if (mipOffsets[ face ][ mipLevel ] + imageSize >= files[ face ].size)
            {
                amountToCopy = files[ face ].size - mipOffsets[ face ][ mipLevel ];
            }

            teMemcpy( stagingData, &files[ face ].data[ mipOffsets[ face ][ mipLevel ] ], amountToCopy );

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = mipLevel;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = mipWidth;
            bufferCopyRegion.imageExtent.height = mipHeight;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = 0;

            vkCmdCopyBufferToImage( cmdBuffer, stagingBuffers[ mipLevel ], tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion );
        }
    }
}

teTexture2D teLoadTexture( const struct teFile& file, unsigned flags, VkDevice device, VkBuffer stagingBuffer, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkQueue graphicsQueue, VkCommandBuffer cmdBuffer, const VkPhysicalDeviceProperties& properties )
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

    if (teStrstr( file.path, ".tga" ) || teStrstr( file.path, ".TGA" ))
    {
        unsigned bitsPerPixel = 24;
        unsigned dataBeginOffset = 0;
        unsigned char* pixelData = nullptr;

        LoadTGA( file, tex.width, tex.height, dataBeginOffset, bitsPerPixel, &pixelData );
        bytesPerPixel = bitsPerPixel == 24 ? 3 : 4;

        mipLevelCount = (flags & teTextureFlags::GenerateMips) ? GetMipLevelCount( tex.width, tex.height ) : 1;
        teAssert( mipLevelCount <= 15 );

        UpdateStagingTexture( file, tex.width, tex.height, dataBeginOffset, bytesPerPixel, format, 0 );
        CreateBaseMip( tex, device, deviceMemoryProperties, graphicsQueue, &stagingBuffer, 1, format, mipLevelCount, file.path, cmdBuffer );
        CreateMipLevels( tex, mipLevelCount, device, graphicsQueue, cmdBuffer );
    }
    else if (teStrstr( file.path, ".dds" ) || teStrstr( file.path, ".DDS" ))
    {
        teTextureFormat bcFormat = teTextureFormat::Invalid;
        unsigned mipOffsets[ 15 ] = {};
        LoadDDS( file, tex.width, tex.height, bcFormat, mipLevelCount, mipOffsets );

        if (!(flags & teTextureFlags::GenerateMips))
        {
            mipLevelCount = 1;
        }

        GetFormatAndBPP( bcFormat, (flags & teTextureFlags::SRGB) ? true : false, format, bytesPerPixel );
        tex.vulkanFormat = format;

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
        SetObjectName( device, (uint64_t)tex.image, VK_OBJECT_TYPE_IMAGE, file.path );

        VkMemoryRequirements memReqs = {};
        vkGetImageMemoryRequirements( device, tex.image, &memReqs );

        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.allocationSize = (memReqs.size + properties.limits.nonCoherentAtomSize - 1) & ~(properties.limits.nonCoherentAtomSize - 1);;
        memAllocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

        VK_CHECK( vkAllocateMemory( device, &memAllocInfo, nullptr, &tex.deviceMemory ) );
        VK_CHECK( vkBindImageMemory( device, tex.image, tex.deviceMemory, 0 ) );

        VkBuffer stagingBuffers[ 15 ] = {};
        VkDeviceMemory stagingMemories[ 15 ] = {};
        const VkDeviceSize bc1BlockSize = (bcFormat == teTextureFormat::BC1) ? 8 : 16;

        for (unsigned i = 0; i < mipLevelCount; ++i)
        {
            const int32_t mipWidth = Max2( tex.width >> i, 1 );
            const int32_t mipHeight = Max2( tex.height >> i, 1 );

            const VkDeviceSize bc1Size = (mipWidth / 4) * (mipHeight / 4) * bc1BlockSize;
            VkDeviceSize imageSize2 = (bcFormat != teTextureFormat::Invalid) ? bc1Size : (mipWidth * mipHeight * bytesPerPixel);

            if (imageSize2 == 0)
            {
                imageSize2 = 16;
            }

            VkBufferCreateInfo mipBufferCreateInfo = {};
            mipBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            mipBufferCreateInfo.size = imageSize2;
            mipBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            mipBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            VK_CHECK( vkCreateBuffer( device, &mipBufferCreateInfo, nullptr, &stagingBuffers[ i ] ) );

            vkGetBufferMemoryRequirements( device, stagingBuffers[ i ], &memReqs );

            memAllocInfo.allocationSize = (memReqs.size + properties.limits.nonCoherentAtomSize - 1) & ~(properties.limits.nonCoherentAtomSize - 1);
            memAllocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
            VK_CHECK( vkAllocateMemory( device, &memAllocInfo, nullptr, &stagingMemories[ i ] ) );
            VK_CHECK( vkBindBufferMemory( device, stagingBuffers[ i ], stagingMemories[ i ], 0 ) );

            void* stagingData;
            VK_CHECK( vkMapMemory( device, stagingMemories[ i ], 0, memAllocInfo.allocationSize, 0, &stagingData ) );
            VkDeviceSize amountToCopy = imageSize2;
            if (mipOffsets[ i ] + imageSize2 >= (unsigned)file.size)
            {
                amountToCopy = file.size - mipOffsets[ i ];
            }

            teMemcpy( stagingData, &file.data[ mipOffsets[ i ] ], amountToCopy );

            VkMappedMemoryRange flushRange = {};
            flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            flushRange.memory = stagingMemories[ i ];
            flushRange.offset = 0;
            flushRange.size = VK_WHOLE_SIZE;
            vkFlushMappedMemoryRanges( device, 1, &flushRange );

            vkUnmapMemory( device, stagingMemories[ i ] );
        }

        VkCommandBufferBeginInfo cmdBufInfo = {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

        SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 0, mipLevelCount, VK_PIPELINE_STAGE_TRANSFER_BIT );

        for (unsigned mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel)
        {
            VkBufferImageCopy mipBufferCopyRegion = {};
            mipBufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipBufferCopyRegion.imageSubresource.mipLevel = mipLevel;
            mipBufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            mipBufferCopyRegion.imageSubresource.layerCount = 1;
            mipBufferCopyRegion.imageExtent.width = Max2( tex.width >> mipLevel, 1 );
            mipBufferCopyRegion.imageExtent.height = Max2( tex.height >> mipLevel, 1 );
            mipBufferCopyRegion.imageExtent.depth = 1;
            mipBufferCopyRegion.bufferOffset = 0;

            vkCmdCopyBufferToImage( cmdBuffer, stagingBuffers[ mipLevel ], tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &mipBufferCopyRegion );
        }

        vkEndCommandBuffer( cmdBuffer );

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        VK_CHECK( vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

        vkDeviceWaitIdle( device );

        VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

        SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 0, mipLevelCount, VK_PIPELINE_STAGE_TRANSFER_BIT );
        SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, mipLevelCount, VK_PIPELINE_STAGE_TRANSFER_BIT );

        vkEndCommandBuffer( cmdBuffer );
        VK_CHECK( vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

        vkDeviceWaitIdle( device );

        for (unsigned i = 0; i < 15; ++i)
        {
            if (stagingBuffers[ i ] != VK_NULL_HANDLE)
            {
                vkDestroyBuffer( device, stagingBuffers[ i ], nullptr );
                vkFreeMemory( device, stagingMemories[ i ], nullptr );
            }
        }
    }
    else
    {
        teAssert( !"unhandled texture type" );
    }

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

teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags, teTextureFilter filter,
                             VkDevice device, VkBuffer* stagingBuffers, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkQueue graphicsQueue, VkCommandBuffer cmdBuffer )
{
    teAssert( textureCount < 100 );
    teAssert( !(flags & teTextureFlags::UAV) );

    teTextureCube outTexture;
    outTexture.index = textureCount++;
    teTextureImpl& tex = textures[ outTexture.index ];
    //tex.filter = filter;
    tex.flags = flags;

    const char* paths[ 6 ] = { negX.path, posX.path, negY.path, posY.path, negZ.path, posZ.path };
    const teFile files[ 6 ] = { negX, posX, negY, posY, negZ, posZ };
    unsigned mipOffsets[ 6 ][ 15 ] = {};
    unsigned bytesPerPixel = 4;
    bool isDDS = false;
    bool isTGA = false;
    teTextureFormat bcFormat = teTextureFormat::Invalid;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    outTexture.format = teTextureFormat::RGBA_sRGB;

    for (unsigned face = 0; face < 6; ++face)
    {
        if (strstr( paths[ face ], ".tga" ) || strstr( paths[ face ], ".TGA" ))
        {
            isTGA = true;

            if (isDDS)
            {
                teAssert( !"cube map contains both .tga and .dds, not supported!" );
                outTexture.index = (unsigned)-1;
                return outTexture;
            }

            unsigned bitsPerPixel = 24;
            unsigned dataBeginOffset = 0;
            unsigned char* pixelData = nullptr;

            LoadTGA( files[ face ], tex.width, tex.height, dataBeginOffset, bitsPerPixel, &pixelData );
            bytesPerPixel = bitsPerPixel == 24 ? 3 : 4;
            tex.mipLevelCount = 1;// (flags & aeTextureFlags::GenerateMips) ? GetMipLevelCount( tex.width, tex.height ) : 1;
            teAssert( tex.mipLevelCount <= 15 );

            UpdateStagingTexture( files[ face ], tex.width, tex.height, dataBeginOffset, bytesPerPixel, format, face );
        }
        else if (strstr( paths[ face ], ".dds" ) || strstr( paths[ face ], ".DDS" ))
        {
            isDDS = true;

            if (isTGA)
            {
                teAssert( !"cube map contains both .tga and .dds, not supported!" );
                outTexture.index = (unsigned)-1;
                return outTexture;
            }

            LoadDDS( files[ face ], tex.width, tex.height, bcFormat, tex.mipLevelCount, mipOffsets[ face ] );

            if (!(flags & teTextureFlags::GenerateMips))
            {
                tex.mipLevelCount = 1;
            }

            GetFormatAndBPP( bcFormat, (flags & teTextureFlags::SRGB) ? true : false, format, bytesPerPixel );
            tex.vulkanFormat = format;
            outTexture.format = bcFormat;
            UpdateStagingTexture( files[ face ], tex.width, tex.height, mipOffsets[ face ][ 0 ], bytesPerPixel, format, face );
        }
    }

    CreateBaseMip( tex, device, deviceMemoryProperties, graphicsQueue, stagingBuffers, 6, format, tex.mipLevelCount, posX.path, cmdBuffer );

    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

    if (isDDS)
    {
        CopyMipmapsFromDDS( tex, bcFormat == teTextureFormat::BC1, format, files, mipOffsets, device, cmdBuffer, deviceMemoryProperties );
    }

    SetImageLayout( cmdBuffer, tex.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6, 0, 1, VK_PIPELINE_STAGE_TRANSFER_BIT );

    VK_CHECK( vkEndCommandBuffer( cmdBuffer ) );

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VK_CHECK( vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

    vkDeviceWaitIdle( device );

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.layerCount = 6;
    viewInfo.subresourceRange.levelCount = tex.mipLevelCount;
    viewInfo.image = tex.image;
    VK_CHECK( vkCreateImageView( device, &viewInfo, nullptr, &tex.view ) );
    SetObjectName( device, (uint64_t)tex.view, VK_OBJECT_TYPE_IMAGE_VIEW, posX.path );

    return outTexture;
}