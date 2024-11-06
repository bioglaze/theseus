#pragma once

enum class BufferViewType { Uint, Ushort, Float2, Float3, Float4 };

struct teBuffer
{
    unsigned index = 0;
    unsigned count = 0;
    unsigned stride = 0;
    unsigned memoryUsage = 0;
};

void CopyBuffer( const teBuffer& source, const teBuffer& destination );
void UpdateStagingBuffer( const teBuffer& buffer, const void* data, unsigned dataBytes, unsigned offset );

