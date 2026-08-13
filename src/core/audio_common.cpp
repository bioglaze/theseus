#include <stdint.h>
#include "audio.h"
#include "file.h"
#include "te_stdlib.h"

void LoadAudioWAV( const char* path, unsigned clipIndex );
void PlayAudioClip( unsigned clipIndex );

struct AudioClip
{
    unsigned internalIndex = 0; // index to audio_wasapi.cpp, audio_mac.cpp or audio_alsa.cpp struct
};

static constexpr unsigned MaxAudioClips = 1000;
static struct AudioClip gAudioClips[ MaxAudioClips ];
static unsigned gAudioClipIndex = 0;

struct AudioSource
{

};

static constexpr unsigned MaxAudioSources = 1000;
static struct AudioSource gAudioSources[ MaxAudioSources ];

teAudioClip teLoadAudioClip( const struct teFile& wavFile )
{
    teAssert( gAudioClipIndex + 1 < MaxAudioClips );

    teAudioClip outClip;
    outClip.index = ++gAudioClipIndex;
    LoadAudioWAV( wavFile.path, outClip.index );

    return outClip;
}

void tePlayAudioClip( teAudioClip clip )
{
    PlayAudioClip( clip.index );
}

static uint16_t ReadU16(const uint8_t* bytes)
{
    return uint16_t(unsigned(bytes[0]) | (unsigned(bytes[1]) << 8));
}

static uint32_t ReadU32(const uint8_t* bytes)
{
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

static bool TagEquals(const uint8_t* bytes, const char* tag)
{
    return bytes[0] == tag[0] && bytes[1] == tag[1] && bytes[2] == tag[2] && bytes[3] == tag[3];
}

// FIXME: int16_t* is wrong for 8-bit data.
int16_t* LoadWAV( teFile& file, int& outSampleRate, int& outChannelCount, int& outFrameCount )
{
    if (!file.data)
    {
        return nullptr;
    }

    constexpr unsigned RiffHeaderSize = 12; // "RIFF" + size + "WAVE"
    constexpr unsigned ChunkHeaderSize = 8; // Chunk id + size

    if (file.size < RiffHeaderSize)
    {
        tePrint( "%s size is too small for a .wav file!\n", file.path );
        return nullptr;
    }

    const uint8_t* bytes = file.data;

    if (!TagEquals( bytes, "RIFF") || !TagEquals(bytes + 8, "WAVE" ))
    {
        tePrint( "%s doesn't contain a valid RIFF/WAVE header!\n", file.path );
        return nullptr;
    }

    // Chunks can come in any order and files often contain LIST, fact etc. before the data chunk, so they're walked through.
    unsigned fmtOffset = 0;
    unsigned fmtSize = 0;
    unsigned dataOffset = 0; // A data chunk can never begin at offset 0, so 0 means "not found".
    unsigned dataSize = 0;
    unsigned offset = RiffHeaderSize;

    while (offset + ChunkHeaderSize <= file.size)
    {
        const uint8_t* chunk = bytes + offset;
        const unsigned chunkSize = ReadU32(chunk + 4);
        const unsigned contentOffset = offset + ChunkHeaderSize;

        if (chunkSize > file.size - contentOffset)
        {
            // Junk after the last chunk is common, so only a truncated fmt or data chunk is worth reporting.
            if (TagEquals( chunk, "fmt " ) || TagEquals( chunk, "data" ))
            {
                tePrint( "%s has a truncated fmt or data chunk!\n", file.path );
            }

            break;
        }

        if (TagEquals( chunk, "fmt " ))
        {
            fmtOffset = contentOffset;
            fmtSize = chunkSize;
        }
        else if (TagEquals( chunk, "data" ))
        {
            dataOffset = contentOffset;
            dataSize = chunkSize;
        }

        offset = contentOffset + chunkSize + (chunkSize & 1); // Chunks are padded to an even size.
    }

    if (fmtOffset == 0 || fmtSize < 16)
    {
        tePrint( "%s doesn't contain a valid fmt chunk!\n", file.path );
        return nullptr;
    }

    if (dataOffset == 0)
    {
        tePrint( "%s doesn't contain a data chunk!\n", file.path );
        return nullptr;
    }

    constexpr unsigned MY_WAVE_FORMAT_PCM = 1;
    //constexpr unsigned MY_WAVE_FORMAT_IEEE_FLOAT = 0x0003;
    constexpr unsigned MY_WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

    const uint8_t* fmt = bytes + fmtOffset;
    unsigned audioFormat = ReadU16(fmt + 0);
    const unsigned channelCount = ReadU16(fmt + 2);
    const unsigned sampleRate = ReadU32(fmt + 4);
    const unsigned bitsPerSample = ReadU16(fmt + 14);

    if (audioFormat == MY_WAVE_FORMAT_EXTENSIBLE)
    {
        if (fmtSize < 40)
        {
            tePrint( "%s is WAVE_FORMAT_EXTENSIBLE but its fmt chunk is too small!\n", file.path );
            return nullptr;
        }

        audioFormat = ReadU16(fmt + 24); // The SubFormat GUID begins with the actual format tag.
    }

    if (audioFormat != MY_WAVE_FORMAT_PCM)
    {
        tePrint( "%s is not PCM, only PCM .wav files are supported!\n", file.path );
        return nullptr;
    }

    if (bitsPerSample != 16)
    {
        tePrint( "%s has %u bits per sample, only 16-bit .wav files are supported!\n", file.path, bitsPerSample );
        return nullptr;
    }

    if (channelCount != 1 && channelCount != 2)
    {
        tePrint( "%s has %u channels, only mono and stereo .wav files are supported!\n", file.path, channelCount );
        return nullptr;
    }

    if (sampleRate == 0)
    {
        tePrint( "%s has a sample rate of 0!\n", file.path );
        return nullptr;
    }

    outChannelCount = int(channelCount);
    outSampleRate = int(sampleRate);
    outFrameCount = int(dataSize / (channelCount * sizeof(int16_t)));

    //tePrint( "sample rate: %d, channel count: %d, frame count: %d, bitsPerSample: %d\n", outSampleRate, outChannelCount, outFrameCount, bitsPerSample );
//tePrint( "sizeof WAVE: %u\n", sizeof( WAVE ));
    return reinterpret_cast< int16_t* >( file.data + dataOffset );
}
