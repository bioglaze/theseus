#import <MetalKit/MetalKit.h>
#include "buffer.h"
#include "te_stdlib.h"

struct BufferImpl
{
    id<MTLBuffer> buffer;
    unsigned sizeBytes = 0;
};

BufferImpl buffers[ 10000 ];
unsigned bufferCount = 0;

Buffer CreateBuffer( id<MTLDevice> device, unsigned dataBytes, bool isStaging, const char* debugName )
{
    Buffer outBuffer;
    outBuffer.index = ++bufferCount;
    
    const unsigned dataBytesNextMultipleOf4 = ((dataBytes + 3) / 4) * 4;

    MTLResourceOptions opt = isStaging ? MTLResourceStorageModeShared : MTLResourceStorageModePrivate;
        
    buffers[ outBuffer.index ].buffer = [device newBufferWithLength:dataBytesNextMultipleOf4
                      options:opt];
    buffers[ outBuffer.index ].sizeBytes = dataBytesNextMultipleOf4;
    buffers[ outBuffer.index ].buffer.label = [NSString stringWithUTF8String:debugName];
    
    return outBuffer;
}

unsigned BufferGetSizeBytes( const Buffer& buffer )
{
    teAssert( buffer.index != 0 );
    
    return buffers[ buffer.index ].sizeBytes;
}

id<MTLBuffer> BufferGetBuffer( const Buffer& buffer )
{
    teAssert( buffer.index != 0 );
    
    return buffers[ buffer.index ].buffer;
}
