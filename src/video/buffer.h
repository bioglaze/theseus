#pragma once

enum class BufferViewType { Uint, Ushort, Float2, Float3, Float4 };

struct Buffer
{
    unsigned index = 0;
    unsigned count = 0;
    unsigned stride = 0;
};

void CopyBuffer( const Buffer& source, const Buffer& destination );
void UpdateStagingBuffer( const Buffer& buffer, const void* data, unsigned dataBytes, unsigned offset );