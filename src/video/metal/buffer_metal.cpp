#include "buffer.h"
#include "te_stdlib.h"
//#define NS_PRIVATE_IMPLEMENTATION
//#define MTL_PRIVATE_IMPLEMENTATION
//#define MTK_PRIVATE_IMPLEMENTATION
//#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
//#include <AppKit/AppKit.hpp>
//#include <MetalKit/MetalKit.hpp>

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
    //buffers[ outBuffer.index ].buffer.label = [NSString stringWithUTF8String:debugName];
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
