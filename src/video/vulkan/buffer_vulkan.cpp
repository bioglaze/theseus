#include <vulkan/vulkan.h>
#include "buffer.h"

void SetObjectName( VkDevice device, uint64_t object, VkObjectType objectType, const char* name );
uint32_t GetMemoryType( uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, VkFlags properties );

struct BufferImpl
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkBufferView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

BufferImpl buffers[ 10000 ];
unsigned bufferCount = 0;

VkBufferView BufferGetView( const Buffer& buffer )
{
    return buffers[ buffer.index ].view;
}

VkBuffer BufferGetBuffer( const Buffer& buffer )
{
    return buffers[ buffer.index ].buffer;
}

VkDeviceMemory BufferGetMemory( const Buffer& buffer )
{
    return buffers[ buffer.index ].memory;
}

Buffer CreateBuffer( VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties, unsigned sizeBytes, VkMemoryPropertyFlags memoryFlags, VkBufferUsageFlags usageFlags, BufferViewType viewType, const char* debugName )
{
    Buffer outBuffer;
    outBuffer.index = ++bufferCount;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeBytes;
    bufferInfo.usage = usageFlags;

    VK_CHECK( vkCreateBuffer( device, &bufferInfo, nullptr, &buffers[ outBuffer.index ].buffer ) );
    SetObjectName( device, (uint64_t)buffers[ outBuffer.index ].buffer, VK_OBJECT_TYPE_BUFFER, debugName );

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements( device, buffers[ outBuffer.index ].buffer, &memReqs );

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = GetMemoryType( memReqs.memoryTypeBits, deviceMemoryProperties, memoryFlags );
    VK_CHECK( vkAllocateMemory( device, &allocInfo, nullptr, &buffers[ outBuffer.index ].memory ) );
    SetObjectName( device, (uint64_t)buffers[ outBuffer.index ].memory, VK_OBJECT_TYPE_DEVICE_MEMORY, debugName );

    VK_CHECK( vkBindBufferMemory( device, buffers[ outBuffer.index ].buffer, buffers[ outBuffer.index ].memory, 0 ) );

    VkBufferViewCreateInfo bufferViewInfo = {};
    bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    bufferViewInfo.buffer = buffers[ outBuffer.index ].buffer;
    bufferViewInfo.range = VK_WHOLE_SIZE;

    if (viewType == BufferViewType::Uint)
    {
        bufferViewInfo.format = VK_FORMAT_R32_UINT;
        outBuffer.count = sizeBytes / 4;
        outBuffer.stride = 4;
    }
    else if (viewType == BufferViewType::Ushort)
    {
        bufferViewInfo.format = VK_FORMAT_R16_UINT;
        outBuffer.count = sizeBytes / 2;
        outBuffer.stride = 2;
    }
    else if (viewType == BufferViewType::Float2)
    {
        bufferViewInfo.format = VK_FORMAT_R32G32_SFLOAT;
        outBuffer.count = sizeBytes / (2 * 4);
        outBuffer.stride = 2 * 4;
    }
    else if (viewType == BufferViewType::Float3)
    {
        bufferViewInfo.format = VK_FORMAT_R32G32B32_SFLOAT;
        outBuffer.count = sizeBytes / (3 * 4);
        outBuffer.stride = 3 * 4;
    }
    else if (viewType == BufferViewType::Float4)
    {
        bufferViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        outBuffer.count = sizeBytes / (4 * 4);
        outBuffer.stride = 4 * 4;
    }
    else
    {
        teAssert( !"Unhandled buffer view type!" );
    }

    if (usageFlags != VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        VK_CHECK( vkCreateBufferView( device, &bufferViewInfo, nullptr, &buffers[ outBuffer.index ].view ) );
        SetObjectName( device, (uint64_t)buffers[ outBuffer.index ].view, VK_OBJECT_TYPE_BUFFER_VIEW, debugName );
    }

    return outBuffer;
}
