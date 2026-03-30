#pragma once

struct teBuffer
{
    unsigned index = 0;
    unsigned memoryUsage = 0;
};

teBuffer CreateBuffer( unsigned size, const char* debugName );
teBuffer CreateStagingBuffer( unsigned size, const char* debugName );
void CopyBuffer( const teBuffer& source, const teBuffer& destination );
void UpdateStagingBuffer( const teBuffer& buffer, const void* data, unsigned dataBytes, unsigned offset );

