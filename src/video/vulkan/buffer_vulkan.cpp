#include <vulkan/vulkan.h>
#include "buffer.h"

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties );
void CopyVulkanBuffer( VkBuffer source, VkBuffer destination, unsigned bufferSize );

struct BufferImpl
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

BufferImpl buffers[ 10000 ];
unsigned bufferCount = 0;

VkBuffer BufferGetBuffer( const teBuffer& buffer )
{
    return buffers[ buffer.index ].buffer;
}

VkDeviceMemory BufferGetMemory( const teBuffer& buffer )
{
    return buffers[ buffer.index ].memory;
}

teBuffer CreateBuffer( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned sizeBytes, VkMemoryPropertyFlags memoryFlags, VkBufferUsageFlags usageFlags, const char* debugName )
{
    teAssert( bufferCount < 10000 );

    teBuffer outBuffer;
    outBuffer.index = ++bufferCount;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeBytes;
    bufferInfo.usage = usageFlags;

    VK_CHECK( vkCreateBuffer( device, &bufferInfo, nullptr, &buffers[ outBuffer.index ].buffer ) );
    SetObjectName( device, (uint64_t)buffers[ outBuffer.index ].buffer, VK_OBJECT_TYPE_BUFFER, debugName );

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements( device, buffers[ outBuffer.index ].buffer, &memReqs );

    outBuffer.sizeBytes = sizeBytes;

    VkMemoryAllocateFlagsInfo flagsInfo = {};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.pNext = (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr;
    allocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, memoryFlags );
    VK_CHECK( vkAllocateMemory( device, &allocInfo, nullptr, &buffers[ outBuffer.index ].memory ) );
    SetObjectName( device, (uint64_t)buffers[ outBuffer.index ].memory, VK_OBJECT_TYPE_DEVICE_MEMORY, debugName );

    VK_CHECK( vkBindBufferMemory( device, buffers[ outBuffer.index ].buffer, buffers[ outBuffer.index ].memory, 0 ) );

    VkBufferViewCreateInfo bufferViewInfo = {};
    bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    bufferViewInfo.buffer = buffers[ outBuffer.index ].buffer;
    bufferViewInfo.range = VK_WHOLE_SIZE;

    return outBuffer;
}

void CopyBuffer( const teBuffer& source, const teBuffer& destination )
{
    teAssert( source.sizeBytes <= destination.sizeBytes );

    CopyVulkanBuffer( buffers[ source.index ].buffer, buffers[ destination.index ].buffer, source.sizeBytes );
}
