#include "buffer.h"
#include "te_stdlib.h"
#include <Metal/Metal.hpp>

struct BufferImpl
{
    MTL::Buffer* buffer;
    unsigned sizeBytes = 0;
};

BufferImpl buffers[ 10000 ];
unsigned bufferCount = 0;

teBuffer CreateBuffer( MTL::Device* device, unsigned dataBytes, bool isStaging, const char* debugName )
{
    teBuffer outBuffer;
    outBuffer.index = ++bufferCount;
    
    const unsigned dataBytesNextMultipleOf4 = ((dataBytes + 3) / 4) * 4;

    MTL::ResourceOptions opt = isStaging ? MTL::ResourceStorageModeShared : MTL::ResourceStorageModePrivate;
        
    buffers[ outBuffer.index ].buffer = device->newBuffer( dataBytesNextMultipleOf4, opt );
    buffers[ outBuffer.index ].sizeBytes = dataBytesNextMultipleOf4;
    buffers[ outBuffer.index ].buffer->setLabel( NS::String::string( debugName, NS::UTF8StringEncoding ) );
    return outBuffer;
}

unsigned BufferGetSizeBytes( const teBuffer& buffer )
{
    teAssert( buffer.index != 0 );
    
    return buffers[ buffer.index ].sizeBytes;
}

MTL::Buffer* BufferGetBuffer( const teBuffer& buffer )
{
    teAssert( buffer.index != 0 );
    
    return buffers[ buffer.index ].buffer;
}
