#import <MetalKit/MetalKit.h>
#include "buffer.h"
#include "te_stdlib.h"

struct BufferImpl
{
    id<MTLBuffer> buffer;
};

BufferImpl buffers[ 10000 ];
unsigned bufferCount = 0;

void CopyBuffer( const Buffer& source, const Buffer& destination )
{
    teAssert( source.stride == destination.stride );
}

Buffer CreateBuffer( id<MTLDevice> device, unsigned dataBytes )
{
    Buffer outBuffer;
    outBuffer.index = ++bufferCount;
    
    const unsigned dataBytesNextMultipleOf4 = ((dataBytes + 3) / 4) * 4;

    buffers[ outBuffer.index ].buffer = [device newBufferWithLength:dataBytesNextMultipleOf4
                      options:MTLResourceStorageModePrivate];

    return outBuffer;
}
