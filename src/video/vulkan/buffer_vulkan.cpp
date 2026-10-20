#include <vulkan/vulkan.h>
#include "buffer.h"

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties );
void CopyVulkanBuffer( VkBuffer source, VkBuffer destination, unsigned bufferSize );

struct BufferImpl
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint64_t address = 0;
};

BufferImpl buffers[ 10000 ];
unsigned bufferCount = 0;

VkBuffer BufferGetBuffer( const teBuffer& buffer )
{
    return buffers[ buffer.index ].buffer;
}

uint64_t BufferGetAddress( const teBuffer& buffer )
{
    return buffers[ buffer.index ].address;
}

VkDeviceMemory BufferGetMemory( const teBuffer& buffer )
{
    return buffers[ buffer.index ].memory;
}

teBuffer CreateBuffer( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned sizeBytes, VkMemoryPropertyFlags memoryFlags, VkBufferUsageFlags usageFlags, const char* debugName )
{
    teAssert( bufferCount + 1 < 10000 );

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

    VkBufferDeviceAddressInfo addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffers[ outBuffer.index ].buffer;

    buffers[ outBuffer.index ].address = (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? vkGetBufferDeviceAddress( device, &addressInfo ) : 0;
    
    return outBuffer;
}

void CopyBuffer( const teBuffer& source, const teBuffer& destination )
{
    teAssert( source.sizeBytes <= destination.sizeBytes );

    CopyVulkanBuffer( buffers[ source.index ].buffer, buffers[ destination.index ].buffer, source.sizeBytes );
}
